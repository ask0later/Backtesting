#include <gtest/gtest.h>

#include <memory>
#include <queue>
#include <utility>
#include <variant>
#include <vector>

#include "MarketEngine/Engine/Engine.hpp"
#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Engine/EventPipeline.hpp"
#include "MarketEngine/Engine/Stages/BookUpdateStage.hpp"
#include "MarketEngine/Engine/Stages/MatchingStage.hpp"
#include "MarketEngine/Engine/Stages/StrategyStage.hpp"
#include "MarketEngine/Execution/MatchingEngine.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"

namespace {

using SparseCtx = me::EngineContext<me::SparseOrderBook>;

struct VectorFeed : me::MarketEventSource<VectorFeed> {
  explicit VectorFeed(std::vector<me::MarketEvent> events)
      : MarketEventSource(std::move(events)) {}
};

class OneShotMarketBuyStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (!sent_ &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {

      ctx.submitOrder(me::MarketOrderRequest{700, me::Side::Buy, 8.0});

      sent_ = true;
    }
  }

private:
  bool sent_ = false;
};

class OneShotRestingBuyStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (!sent_ &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {

      ctx.submitOrder(me::AddOrderRequest{
          me::Order{800, 100.5, 6.0, me::Side::Buy, event.timestamp}});

      sent_ = true;
    }
  }

private:
  bool sent_ = false;
};

const me::Fill *findFill(const std::vector<me::ExecutionEvent> &events,
                         me::OrderId orderId) {
  for (const auto &event : events) {
    if (std::holds_alternative<me::Fill>(event)) {
      const auto &fill = std::get<me::Fill>(event);

      if (fill.orderId == orderId) {
        return &fill;
      }
    }
  }

  return nullptr;
}

const me::ExecutionReport *
findReport(const std::vector<me::ExecutionEvent> &events, me::OrderId orderId,
           me::ExecutionStatus status) {
  for (const auto &event : events) {
    if (std::holds_alternative<me::ExecutionReport>(event)) {
      const auto &report = std::get<me::ExecutionReport>(event);

      if (report.orderId == orderId && report.status == status) {
        return &report;
      }
    }
  }

  return nullptr;
}

} // namespace

TEST(MatchingEngineTest, LimitCrossesBestAskWithoutMutatingBook) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{10, 101.0, 30.0, me::Side::Sell}});
  me::MatchingEngine<me::SparseOrderBook> engine;
  auto result = engine.submit(
      me::AddOrderRequest{me::Order{1, 102.0, 10.0, me::Side::Buy}}, book, 2);
  ASSERT_EQ(result.events.size(), 2u);

  const auto *fill = findFill(result.events, 1);
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->orderId, 1u);
  EXPECT_EQ(fill->side, me::Side::Buy);
  EXPECT_DOUBLE_EQ(fill->price, 101.0);
  EXPECT_DOUBLE_EQ(fill->qty, 10.0);

  const auto *report =
      findReport(result.events, 1, me::ExecutionStatus::Filled);

  ASSERT_NE(report, nullptr);
  EXPECT_DOUBLE_EQ(report->remainingQty, 0.0);
  EXPECT_TRUE(engine.activeOrders().empty());
  auto asks = book.asks();
  ASSERT_NE(asks.first, asks.second);
  EXPECT_DOUBLE_EQ(asks.first->second, 30.0);
}

TEST(MatchingEngineTest, PartiallyFilledLimitRestsRemainder) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{10, 101.0, 5.0, me::Side::Sell}});
  me::MatchingEngine<me::SparseOrderBook> engine;
  auto result = engine.submit(
      me::AddOrderRequest{me::Order{1, 102.0, 12.0, me::Side::Buy}}, book, 2);

  const auto *fill = findFill(result.events, 1);
  ASSERT_NE(fill, nullptr);
  EXPECT_DOUBLE_EQ(fill->qty, 5.0);

  auto *active = engine.findActiveOrder(1);
  ASSERT_NE(active, nullptr);
  EXPECT_DOUBLE_EQ(active->remainingQty, 7.0);

  const auto *report =
      findReport(result.events, 1, me::ExecutionStatus::PartiallyFilled);
  ASSERT_NE(report, nullptr);
  EXPECT_DOUBLE_EQ(report->remainingQty, 7.0);
}

TEST(MatchingEngineTest, MarketOrderFillsBestPriceAndDoesNotRest) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{10, 99.0, 20.0, me::Side::Buy}});
  me::MatchingEngine<me::SparseOrderBook> engine;
  auto result =
      engine.submit(me::MarketOrderRequest{44, me::Side::Sell, 7.0}, book, 3);

  const auto *fill = findFill(result.events, 44);
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->orderId, 44u);
  EXPECT_EQ(fill->side, me::Side::Sell);
  EXPECT_DOUBLE_EQ(fill->price, 99.0);
  EXPECT_DOUBLE_EQ(fill->qty, 7.0);

  const auto *report =
      findReport(result.events, 44, me::ExecutionStatus::Filled);
  ASSERT_NE(report, nullptr);
  EXPECT_DOUBLE_EQ(report->remainingQty, 0.0);
  EXPECT_TRUE(engine.activeOrders().empty());
}

TEST(MatchingEngineTest, CancelAndModifyOperateOnParticipantOrdersOnly) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{10, 101.0, 20.0, me::Side::Sell}});
  me::MatchingEngine<me::SparseOrderBook> engine;
  engine.submit(me::AddOrderRequest{me::Order{1, 99.0, 10.0, me::Side::Buy}},
                book, 1);
  auto modifyResult =
      engine.submit(me::ModifyOrderRequest{1, 6.0, 0.0}, book, 2);

  auto *active = engine.findActiveOrder(1);
  ASSERT_NE(active, nullptr);
  EXPECT_DOUBLE_EQ(active->price, 99.0);
  EXPECT_DOUBLE_EQ(active->remainingQty, 6.0);

  const auto *modifyReport =
      findReport(modifyResult.events, 1, me::ExecutionStatus::PartiallyFilled);
  ASSERT_NE(modifyReport, nullptr);
  EXPECT_DOUBLE_EQ(modifyReport->remainingQty, 6.0);

  auto cancelResult = engine.submit(me::CancelOrderRequest{1}, book, 3);
  EXPECT_EQ(engine.findActiveOrder(1), nullptr);

  const auto *cancelReport =
      findReport(cancelResult.events, 1, me::ExecutionStatus::Canceled);
  ASSERT_NE(cancelReport, nullptr);
  auto asks = book.asks();
  ASSERT_NE(asks.first, asks.second);
  EXPECT_DOUBLE_EQ(asks.first->second, 20.0);
}

TEST(MatchingStageTest, ReplayStrategyAndMatchingProduceFill) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::SnapshotUpdateEvent snap;
  snap.newBids = {{100.0, 10.0}};
  snap.newAsks = {{101.0, 20.0}};
  VectorFeed feed({me::MarketEvent{1, snap}});
  OneShotMarketBuyStrategy strategy;
  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  ASSERT_EQ(engine.context().executionEvents.size(), 2u);

  const auto *fill = findFill(engine.context().executionEvents, 700);
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->orderId, 700u);
  EXPECT_DOUBLE_EQ(fill->price, 101.0);
  EXPECT_DOUBLE_EQ(fill->qty, 8.0);
  EXPECT_TRUE(engine.context().pendingModelEvents.empty());

  auto asks = book.asks();
  ASSERT_NE(asks.first, asks.second);
  EXPECT_DOUBLE_EQ(asks.first->second, 20.0);
}

TEST(MatchingStageTest, RestingLimitOrderMatchesAfterLaterBookUpdate) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::SnapshotUpdateEvent firstSnap;
  firstSnap.newBids = {{100.0, 10.0}};
  firstSnap.newAsks = {{101.0, 20.0}};
  me::SnapshotUpdateEvent secondSnap;
  secondSnap.newBids = {{100.0, 10.0}};
  secondSnap.newAsks = {{100.5, 20.0}};
  VectorFeed feed(
      {me::MarketEvent{1, firstSnap}, me::MarketEvent{2, secondSnap}});
  OneShotRestingBuyStrategy strategy;
  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  const auto *fill = findFill(engine.context().executionEvents, 800);
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->orderId, 800u);
  EXPECT_DOUBLE_EQ(fill->price, 100.5);
  EXPECT_DOUBLE_EQ(fill->qty, 6.0);
}