#include <gtest/gtest.h>

#include <memory>
#include <queue>
#include <vector>

#include "MarketEngine/Engine/Engine.hpp"
#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Engine/EventPipeline.hpp"
#include "MarketEngine/Engine/Stages/BookUpdateStage.hpp"
#include "MarketEngine/Engine/Stages/StrategyStage.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"

namespace {

using SparseCtx = me::EngineContext<me::SparseOrderBook>;

struct VectorFeed : me::MarketEventSource<VectorFeed> {
  explicit VectorFeed(std::vector<me::MarketEvent> &&events)
      : MarketEventSource(std::move(events)) {}
};

class SnapshotBuyStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  size_t callCount = 0;

  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    ++callCount;
    if (auto *snap = std::get_if<me::SnapshotUpdateEvent>(&event.event)) {
      if (!snap->newBids.empty()) {
        me::Order order;
        order.id = nextOrderId_++;
        order.price = snap->newBids.begin()->first - 0.001;
        order.qty = 15.0;
        order.side = me::Side::Buy;
        ctx.submitOrder(me::AddOrderRequest{order});
      }
    }
  }

private:
  int nextOrderId_ = 1;
};

class MultiOrderStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  size_t callCount = 0;

  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    ++callCount;
    if (std::get_if<me::SnapshotUpdateEvent>(&event.event)) {
      me::Order buy{101, 95.0, 10.0, me::Side::Buy};
      me::Order sell{102, 105.0, 5.0, me::Side::Sell};
      ctx.submitOrder(me::AddOrderRequest{buy});
      ctx.submitOrder(me::AddOrderRequest{sell});
    }
  }
};

class BookBasedStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  size_t callCount = 0;

  void onMarketEvent(const me::MarketEvent &,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    ++callCount;
    auto bestBid = ctx.book().getBestBid();
    auto bestAsk = ctx.book().getBestAsk();
    if (bestBid.has_value() && bestAsk.has_value() &&
        *bestAsk - *bestBid > 1.0) {
      me::Order order;
      order.id = 999;
      order.price = (*bestAsk + *bestBid) / 2.0;
      order.qty = 10.0;
      order.side = me::Side::Buy;
      ctx.submitOrder(me::AddOrderRequest{order});
    }
  }
};

class CancelModifyStrategy final
    : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  bool cancelSent = false;
  bool modifySent = false;

  void onMarketEvent(const me::MarketEvent &,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    ctx.submitOrder(me::CancelOrderRequest{42});
    ctx.submitOrder(me::ModifyOrderRequest{43, 75.0});
    cancelSent = true;
    modifySent = true;
  }
};

class SilentStrategy final : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &,
                     me::StrategyContext<me::SparseOrderBook> &) override {}
};

} // namespace

TEST(StrategyTest, GeneratesOrderOnSnapshot) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap;
  snap.newBids = {{100.0, 50.0}};
  snap.newAsks = {{101.0, 30.0}};
  me::MarketEvent ev{42, snap};
  VectorFeed feed({ev});

  SnapshotBuyStrategy strategy;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(strategy.callCount, 1u);

  const auto &orders = engine.context().pendingModelEvents;
  ASSERT_EQ(orders.size(), 1u);

  const auto &front = orders.front();
  auto *addReq = std::get_if<me::AddOrderRequest>(&front);
  ASSERT_NE(addReq, nullptr);

  EXPECT_EQ(addReq->order.side, me::Side::Buy);
  EXPECT_DOUBLE_EQ(addReq->order.price, 99.999);
  EXPECT_DOUBLE_EQ(addReq->order.qty, 15.0);
}

TEST(StrategyTest, NoOrderOnNonSnapshot) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::MarketEvent ev{10, me::ExecuteMarketEvent{5.0, me::Side::Sell, 0.0}};
  VectorFeed feed({ev});

  SnapshotBuyStrategy strategy;

  me::EventPipeline<SparseCtx, me::StrategyStage<me::SparseOrderBook>> pipeline(
      me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(strategy.callCount, 1u);
  EXPECT_TRUE(engine.context().pendingModelEvents.empty());
}

TEST(StrategyTest, MultipleOrdersSingleEvent) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap;
  snap.newBids = {{100.0, 50.0}};
  snap.newAsks = {{101.0, 30.0}};
  me::MarketEvent ev{1, snap};
  VectorFeed feed({ev});

  MultiOrderStrategy strategy;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(strategy.callCount, 1u);

  const auto &orders = engine.context().pendingModelEvents;
  ASSERT_EQ(orders.size(), 2u);

  {
    const auto &front = orders.front();
    auto *req = std::get_if<me::AddOrderRequest>(&front);
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->order.side, me::Side::Buy);
    EXPECT_DOUBLE_EQ(req->order.price, 95.0);
  }

  auto events = orders.snapshot();
  auto *req0 = std::get_if<me::AddOrderRequest>(&events[0]);
  auto *req1 = std::get_if<me::AddOrderRequest>(&events[1]);
  ASSERT_TRUE(req0 && req1);
  EXPECT_EQ(req0->order.side, me::Side::Buy);
  EXPECT_DOUBLE_EQ(req0->order.price, 95.0);
  EXPECT_EQ(req1->order.side, me::Side::Sell);
  EXPECT_DOUBLE_EQ(req1->order.price, 105.0);
}

TEST(StrategyTest, ReactsToBookAfterMarketTrade) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap;
  snap.newBids = {{100.0, 10.0}};
  snap.newAsks = {{110.0, 20.0}};
  me::MarketEvent ev{1, snap};
  VectorFeed feed({ev});

  BookBasedStrategy strategy;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(strategy.callCount, 1u);

  const auto &orders = engine.context().pendingModelEvents;
  ASSERT_EQ(orders.size(), 1u);

  auto *req = std::get_if<me::AddOrderRequest>(&orders.front());
  ASSERT_NE(req, nullptr);
  EXPECT_EQ(req->order.side, me::Side::Buy);
  EXPECT_DOUBLE_EQ(req->order.price, 105.0);
  EXPECT_DOUBLE_EQ(req->order.qty, 10.0);
}

TEST(StrategyTest, SendsCancelAndModify) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::MarketEvent ev{1, me::AddOrderEvent{}};
  VectorFeed feed({ev});

  CancelModifyStrategy strategy;

  me::EventPipeline<SparseCtx, me::StrategyStage<me::SparseOrderBook>> pipeline(
      me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_TRUE(strategy.cancelSent);
  EXPECT_TRUE(strategy.modifySent);

  const auto &orders = engine.context().pendingModelEvents;
  ASSERT_EQ(orders.size(), 2u);

  auto events = orders.snapshot();
  EXPECT_TRUE(std::holds_alternative<me::CancelOrderRequest>(events[0]));
  EXPECT_TRUE(std::holds_alternative<me::ModifyOrderRequest>(events[1]));

  EXPECT_EQ(std::get<me::CancelOrderRequest>(events[0]).id, 42);
  EXPECT_EQ(std::get<me::ModifyOrderRequest>(events[1]).id, 43);
  EXPECT_DOUBLE_EQ(std::get<me::ModifyOrderRequest>(events[1]).newQty, 75.0);
}

TEST(StrategyTest, MultipleEventsAccumulateOrders) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap1;
  snap1.newBids = {{100.0, 10.0}};
  snap1.newAsks = {{101.0, 5.0}};
  me::MarketEvent ev1{1, snap1};

  me::SnapshotUpdateEvent snap2;
  snap2.newBids = {{99.0, 20.0}};
  snap2.newAsks = {{102.0, 15.0}};
  me::MarketEvent ev2{2, snap2};

  VectorFeed feed({ev1, ev2});

  SnapshotBuyStrategy strategy;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(strategy.callCount, 2u);

  const auto &orders = engine.context().pendingModelEvents;
  ASSERT_EQ(orders.size(), 2u);

  auto events = orders.snapshot();
  ASSERT_TRUE(std::holds_alternative<me::AddOrderRequest>(events[0]));
  ASSERT_TRUE(std::holds_alternative<me::AddOrderRequest>(events[1]));

  double price0 = std::get<me::AddOrderRequest>(events[0]).order.price;
  double price1 = std::get<me::AddOrderRequest>(events[1]).order.price;
  EXPECT_DOUBLE_EQ(price0, 99.999);
  EXPECT_DOUBLE_EQ(price1, 98.999);
}

TEST(StrategyTest, StrategyIgnoringEventsLeavesEmptyQueue) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::MarketEvent ev{1, me::CancelOrderEvent{}};
  VectorFeed feed({ev});

  SilentStrategy strategy;

  me::EventPipeline<SparseCtx, me::StrategyStage<me::SparseOrderBook>> pipeline(
      me::StrategyStage<me::SparseOrderBook>{strategy});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_TRUE(engine.context().pendingModelEvents.empty());
}