#include <gtest/gtest.h>

#include "MarketEngine/Engine/Engine.hpp"
#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Engine/EventPipeline.hpp"
#include "MarketEngine/Engine/Stages/BookUpdateStage.hpp"
#include "MarketEngine/Engine/Stages/MatchingStage.hpp"
#include "MarketEngine/Engine/Stages/MetricsStage.hpp"
#include "MarketEngine/Engine/Stages/StrategyStage.hpp"
#include "MarketEngine/Execution/MatchingEngine.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"
#include "MarketEngine/Metrics/MetricsEngine.hpp"
#include "MarketEngine/Metrics/MetricsEngineContext.hpp"
#include "MarketEngine/Metrics/Portfolio.hpp"
#include "MarketEngine/Strategy/StrategyAdapter.hpp"

namespace {

constexpr double kEps = 1e-9;

using SparseCtx = me::EngineContext<me::SparseOrderBook>;

struct VectorFeed : me::MarketEventSource<VectorFeed> {
  explicit VectorFeed(std::vector<me::MarketEvent> events)
      : MarketEventSource(std::move(events)) {}
};

class MarketBuyOnceStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (!sent_ &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {
      ctx.submitOrder(me::MarketOrderRequest{100, me::Side::Buy, 5.0});
      sent_ = true;
    }
  }

private:
  bool sent_ = false;
};

class LimitPartialStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (!sent_ &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {
      ctx.submitOrder(me::AddOrderRequest{
          me::Order{200, 101.0, 10.0, me::Side::Buy, event.timestamp}});
      sent_ = true;
    }
  }

private:
  bool sent_ = false;
};

class CancelLaterStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (stage_ == 0 &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {
      ctx.submitOrder(me::AddOrderRequest{
          me::Order{300, 99.0, 10.0, me::Side::Buy, event.timestamp}});
      ++stage_;
    } else if (stage_ == 1 &&
               std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {
      ctx.submitOrder(me::CancelOrderRequest{300});
      ++stage_;
    }
  }

private:
  int stage_ = 0;
};

class MarketSellOnceStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &event,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    if (!sent_ &&
        std::holds_alternative<me::SnapshotUpdateEvent>(event.event)) {
      ctx.submitOrder(me::MarketOrderRequest{101, me::Side::Sell, 5.0});
      sent_ = true;
    }
  }

private:
  bool sent_ = false;
};

class MarketRejectedStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
public:
  void onMarketEvent(const me::MarketEvent &,
                     me::StrategyContext<me::SparseOrderBook> &ctx) override {
    ctx.submitOrder(me::MarketOrderRequest{999, me::Side::Buy, 5.0});
  }
};

} // namespace

TEST(PortfolioTest, BuySellRoundTrip) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 100.0, 10.0});
  const auto s1 = p.snapshot();
  EXPECT_DOUBLE_EQ(s1.inventory, 10.0);
  EXPECT_DOUBLE_EQ(s1.avgEntryPrice, 100.0);
  EXPECT_DOUBLE_EQ(s1.realizedPnl, 0.0);
  EXPECT_NEAR(s1.cash, -1000.0, kEps);

  p.applyFill({2, me::Side::Sell, 110.0, 10.0});
  const auto s2 = p.snapshot();
  EXPECT_DOUBLE_EQ(s2.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s2.realizedPnl, 100.0);
  EXPECT_NEAR(s2.cash, 100.0, kEps);
  EXPECT_DOUBLE_EQ(s2.turnover, 100.0 * 10.0 + 110.0 * 10.0);
  EXPECT_DOUBLE_EQ(s2.buyVolume, 10.0);
  EXPECT_DOUBLE_EQ(s2.sellVolume, 10.0);
  EXPECT_EQ(s2.fillCount, 2u);
  EXPECT_DOUBLE_EQ(s2.maxAbsInventory, 10.0);
}

TEST(PortfolioTest, PartialClose) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 100.0, 10.0});
  p.applyFill({2, me::Side::Sell, 110.0, 4.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 6.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 100.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 40.0);
  EXPECT_NEAR(s.cash, -1000.0 + 440.0, kEps);
  EXPECT_DOUBLE_EQ(s.maxAbsInventory, 10.0);
}

TEST(PortfolioTest, FlipLongToShort) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 100.0, 10.0});
  p.applyFill({2, me::Side::Sell, 200.0, 25.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, -15.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 200.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 10.0 * (200.0 - 100.0));
}

TEST(PortfolioTest, FlipShortToLong) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Sell, 100.0, 10.0});
  p.applyFill({2, me::Side::Buy, 90.0, 25.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 15.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 90.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 10.0 * (100.0 - 90.0));
}

TEST(PortfolioTest, MarkToMarketUnrealized) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 100.0, 10.0});
  p.setMarkPrice(120.0);
  auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.unrealizedPnl, 200.0);
  EXPECT_DOUBLE_EQ(s.totalPnl, 200.0);
  ASSERT_TRUE(s.markPrice.has_value());
  EXPECT_DOUBLE_EQ(*s.markPrice, 120.0);

  p.applyFill({2, me::Side::Sell, 110.0, 10.0});
  s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.realizedPnl, 100.0);
  EXPECT_DOUBLE_EQ(s.unrealizedPnl, 0.0);
  EXPECT_DOUBLE_EQ(s.totalPnl, 100.0);
}

TEST(PortfolioTest, ShortPartialCover) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Sell, 100.0, 10.0});
  p.applyFill({2, me::Side::Buy, 90.0, 4.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, -6.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 100.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 40.0);
}

TEST(PortfolioTest, MultipleBuysAveragePrice) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 100.0, 10.0});
  p.applyFill({2, me::Side::Buy, 120.0, 10.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 20.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 110.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 0.0);
  EXPECT_NEAR(s.cash, -2200.0, kEps);
  EXPECT_DOUBLE_EQ(s.buyVolume, 20.0);
  EXPECT_DOUBLE_EQ(s.sellVolume, 0.0);
}

TEST(PortfolioTest, ShortMultipleCovers) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Sell, 50.0, 10.0});
  p.applyFill({2, me::Side::Buy, 45.0, 4.0});
  p.applyFill({3, me::Side::Buy, 48.0, 6.0});
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 4.0 * (50.0 - 45.0) + 6.0 * (50.0 - 48.0));
  EXPECT_DOUBLE_EQ(s.maxAbsInventory, 10.0);
}

TEST(PortfolioTest, MarkPriceNoPosition) {
  me::Portfolio p;
  p.setMarkPrice(500.0);
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.unrealizedPnl, 0.0);
  EXPECT_DOUBLE_EQ(s.totalPnl, 0.0);
  ASSERT_TRUE(s.markPrice.has_value());
  EXPECT_DOUBLE_EQ(*s.markPrice, 500.0);
}

TEST(PortfolioTest, ResetClearsState) {
  me::Portfolio p;
  p.applyFill({1, me::Side::Buy, 10.0, 5.0});
  p.reset();
  const auto s = p.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 0.0);
  EXPECT_DOUBLE_EQ(s.cash, 0.0);
  EXPECT_EQ(s.fillCount, 0u);
}

TEST(MetricsEngineTest, MarkToMarketThroughContext) {
  me::MetricsEngineContext ctx;
  me::MetricsEngine engine(ctx);
  engine.observe(me::Fill{1, me::Side::Buy, 100.0, 10.0});
  engine.setMarkToMarket(130.0);
  const auto s = ctx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.unrealizedPnl, 300.0);
  ASSERT_TRUE(ctx.lastMarkPrice.has_value());
  EXPECT_DOUBLE_EQ(*ctx.lastMarkPrice, 130.0);
}

TEST(MetricsEngineTest, ExecutionReportDoesNotChangePortfolio) {
  me::MetricsEngineContext ctx;
  me::MetricsEngine engine(ctx);
  engine.observe(me::ExecutionReport{1, me::ExecutionStatus::Accepted, 10.0});
  EXPECT_EQ(ctx.portfolio.fillCount(), 0u);
  EXPECT_DOUBLE_EQ(ctx.portfolio.turnover(), 0.0);
}

TEST(MetricsEngineTest, SnapshotHistoryWhenRecording) {
  me::MetricsEngineContext ctx;
  ctx.recordSnapshots = true;
  me::MetricsEngine engine(ctx);
  engine.observe(me::Fill{1, me::Side::Buy, 10.0, 1.0});
  engine.observe(me::Fill{2, me::Side::Buy, 20.0, 1.0});
  ASSERT_EQ(ctx.snapshotHistory.size(), 2u);
  EXPECT_DOUBLE_EQ(ctx.snapshotHistory[0].inventory, 1.0);
  EXPECT_DOUBLE_EQ(ctx.snapshotHistory[1].inventory, 2.0);
}

TEST(MetricsEngineTest, IgnoresUnknownExecutionEvent) {
  me::MetricsEngineContext ctx;
  me::MetricsEngine engine(ctx);
  engine.observe(me::ExecutionReport{1, me::ExecutionStatus::Rejected, 0.0});
  EXPECT_EQ(ctx.portfolio.fillCount(), 0u);
}

TEST(MetricsStageTest, FeedsExecutionEventsToEngine) {
  me::SparseOrderBook book;
  me::EngineContext<me::SparseOrderBook> ctx{.book = book};
  ctx.executionEvents.push_back(me::Fill{1, me::Side::Buy, 50.0, 2.0});
  ctx.executionEvents.push_back(me::Fill{2, me::Side::Sell, 60.0, 2.0});

  me::MetricsEngineContext mctx;
  me::MetricsEngine engine(mctx);
  me::MetricsStage<me::SparseOrderBook> stage(engine);

  const me::Order o{1, 1.0, 1.0, me::Side::Buy, 0};
  const me::MarketEvent ev{0, me::AddOrderEvent{o}};
  stage.process(ev, ctx);

  EXPECT_EQ(stage.processedExecutionEvents(), 2u);
  const auto s = mctx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 20.0);
}

TEST(MetricsStageTest, EmptyExecutionEventsSafe) {
  me::SparseOrderBook book;
  me::EngineContext<me::SparseOrderBook> ctx{.book = book};

  me::MetricsEngineContext mctx;
  me::MetricsEngine engine(mctx);
  me::MetricsStage<me::SparseOrderBook> stage(engine);

  const me::Order o{1, 1.0, 1.0, me::Side::Buy, 0};
  const me::MarketEvent ev{0, me::AddOrderEvent{o}};
  stage.process(ev, ctx);

  EXPECT_EQ(stage.processedExecutionEvents(), 0u);
  EXPECT_EQ(mctx.portfolio.fillCount(), 0u);
}

TEST(MetricsPipelineTest, MarketOrderFillUpdatesPortfolio) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap;
  snap.newBids = {{100.0, 10.0}};
  snap.newAsks = {{101.0, 20.0}};
  VectorFeed feed({me::MarketEvent{1, snap}});

  MarketBuyOnceStrategy strategy;

  me::MetricsEngineContext metricsCtx;
  me::MetricsEngine metricsEngine(metricsCtx);

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>,
                    me::MetricsStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{},
               me::MetricsStage<me::SparseOrderBook>{metricsEngine});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  const auto s = metricsCtx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 5.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 101.0);
  EXPECT_NEAR(s.cash, -505.0, 1e-9);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 0.0);
  EXPECT_DOUBLE_EQ(s.buyVolume, 5.0);
  EXPECT_DOUBLE_EQ(s.sellVolume, 0.0);
  EXPECT_EQ(s.fillCount, 1u);
}

TEST(MetricsPipelineTest, PartialFillThenMatchOnNextBook) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap1;
  snap1.newBids = {{100.0, 10.0}};
  snap1.newAsks = {{101.0, 3.0}};

  me::SnapshotUpdateEvent snap2;
  snap2.newBids = {{100.0, 10.0}};
  snap2.newAsks = {{101.0, 10.0}};

  VectorFeed feed({me::MarketEvent{1, snap1}, me::MarketEvent{2, snap2}});

  LimitPartialStrategy strategy;

  me::MetricsEngineContext metricsCtx;
  me::MetricsEngine metricsEngine(metricsCtx);

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>,
                    me::MetricsStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{},
               me::MetricsStage<me::SparseOrderBook>{metricsEngine});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  const auto s = metricsCtx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 10.0);
  EXPECT_DOUBLE_EQ(s.avgEntryPrice, 101.0);
  EXPECT_NEAR(s.cash, -1010.0, 1e-9);
  EXPECT_EQ(s.fillCount, 2u);
  EXPECT_DOUBLE_EQ(s.buyVolume, 10.0);
}

TEST(MetricsPipelineTest, ExecutionReportIgnoredByPortfolio) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap;
  snap.newBids = {};
  snap.newAsks = {};
  VectorFeed feed({me::MarketEvent{1, snap}});

  MarketRejectedStrategy strategy;

  me::MetricsEngineContext metricsCtx;
  me::MetricsEngine metricsEngine(metricsCtx);

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>,
                    me::MetricsStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{},
               me::MetricsStage<me::SparseOrderBook>{metricsEngine});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  const auto s = metricsCtx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s.cash, 0.0);
  EXPECT_EQ(s.fillCount, 0u);
}

TEST(MetricsPipelineTest, CancelOrderDoesNotAffectPortfolio) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap1;
  snap1.newBids = {{99.0, 10.0}};
  snap1.newAsks = {{101.0, 10.0}};
  me::SnapshotUpdateEvent snap2;
  snap2.newBids = {{99.0, 10.0}};
  snap2.newAsks = {{101.0, 10.0}};

  VectorFeed feed({me::MarketEvent{1, snap1}, me::MarketEvent{2, snap2}});

  CancelLaterStrategy strategy;

  me::MetricsEngineContext metricsCtx;
  me::MetricsEngine metricsEngine(metricsCtx);

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>,
                    me::MetricsStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{},
               me::MetricsStage<me::SparseOrderBook>{metricsEngine});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(metricsCtx.portfolio.fillCount(), 0u);
  EXPECT_DOUBLE_EQ(metricsCtx.portfolio.snapshot().inventory, 0.0);
}

TEST(MetricsPipelineTest, BuyAndSellRoundTrip) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::SnapshotUpdateEvent snap1;
  snap1.newBids = {{100.0, 10.0}};
  snap1.newAsks = {{101.0, 20.0}};
  me::SnapshotUpdateEvent snap2;
  snap2.newBids = {{102.0, 10.0}};
  snap2.newAsks = {{103.0, 20.0}};

  VectorFeed feed({me::MarketEvent{1, snap1}, me::MarketEvent{2, snap2}});

  class BuyThenSellStrategy : public me::StrategyAdapter<me::SparseOrderBook> {
  public:
    void onMarketEvent(const me::MarketEvent &event,
                       me::StrategyContext<me::SparseOrderBook> &ctx) override {
      if (event.timestamp == 1) {
        ctx.submitOrder(me::MarketOrderRequest{1, me::Side::Buy, 5.0});
      } else if (event.timestamp == 2) {
        ctx.submitOrder(me::MarketOrderRequest{2, me::Side::Sell, 5.0});
      }
    }
  };
  BuyThenSellStrategy strategy;

  me::MetricsEngineContext metricsCtx;
  me::MetricsEngine metricsEngine(metricsCtx);

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    me::StrategyStage<me::SparseOrderBook>,
                    me::MatchingStage<me::SparseOrderBook>,
                    me::MetricsStage<me::SparseOrderBook>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               me::StrategyStage<me::SparseOrderBook>{strategy},
               me::MatchingStage<me::SparseOrderBook>{},
               me::MetricsStage<me::SparseOrderBook>{metricsEngine});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  const auto s = metricsCtx.portfolio.snapshot();
  EXPECT_DOUBLE_EQ(s.inventory, 0.0);
  EXPECT_DOUBLE_EQ(s.realizedPnl, 5.0);
  EXPECT_DOUBLE_EQ(s.buyVolume, 5.0);
  EXPECT_DOUBLE_EQ(s.sellVolume, 5.0);
  EXPECT_EQ(s.fillCount, 2u);
}