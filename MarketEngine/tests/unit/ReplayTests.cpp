#include <gtest/gtest.h>

#include <limits>

#include "MarketEngine/Engine/Engine.hpp"
#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Engine/EventPipeline.hpp"
#include "MarketEngine/Engine/Stages/BookUpdateStage.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"

namespace {
constexpr int kSmallLobRows = 25;
constexpr int kSmallTradeRows = 29;

using SparseCtx = me::EngineContext<me::SparseOrderBook>;

template <typename Ctx> struct TypeMonitorStage {
  size_t &total;
  me::TimestampNsT &lastTs;
  bool &sawSnapshot;
  bool &sawTrade;
  size_t &snapshotCount;
  size_t &tradeCount;

  void process(const me::MarketEvent &e, Ctx &ctx) {
    ++total;
    lastTs = ctx.now;
    std::visit(
        [&](const auto &v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, me::SnapshotUpdateEvent>) {
            sawSnapshot = true;
            ++snapshotCount;
          } else if constexpr (std::is_same_v<T, me::ExecuteMarketEvent>) {
            sawTrade = true;
            ++tradeCount;
          }
        },
        e.event);
  }
};

template <typename Ctx> struct OrderCheckStage {
  me::TimestampNsT &prev;
  void process(const me::MarketEvent &e, Ctx &) {
    EXPECT_GE(e.timestamp, prev);
    prev = e.timestamp;
  }
};

} // namespace

TEST(EngineIntegration, LobSnapshotFullProcessing) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::LobSnapshotEventSource feed("data/small_lob.csv");

  ASSERT_EQ(feed.remaining(), kSmallLobRows);

  size_t total = 0;
  me::TimestampNsT lastTs = 0;
  bool sawSnap = false, sawTrade = false;
  size_t snapCnt = 0, tradeCnt = 0;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    TypeMonitorStage<SparseCtx>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               TypeMonitorStage<SparseCtx>{total, lastTs, sawSnap, sawTrade,
                                           snapCnt, tradeCnt});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(total, kSmallLobRows);
  EXPECT_TRUE(sawSnap);
  EXPECT_FALSE(sawTrade);
  EXPECT_EQ(snapCnt, kSmallLobRows);
  EXPECT_EQ(tradeCnt, 0u);
  EXPECT_EQ(lastTs, 1722470414467985LL);

  auto bestBid = book.getBestBid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_DOUBLE_EQ(*bestBid, 0.0110263);
  auto bestAsk = book.getBestAsk();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_DOUBLE_EQ(*bestAsk, 0.0110264);

  auto [bidIt, bidEnd] = book.bids();
  ASSERT_NE(bidIt, bidEnd);
  EXPECT_DOUBLE_EQ(bidIt->second, 1572.0);
  auto [askIt, askEnd] = book.asks();
  ASSERT_NE(askIt, askEnd);
  EXPECT_DOUBLE_EQ(askIt->second, 487008.0);
}

TEST(EngineIntegration, TradesSourceProcessing) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::TradesEventSource feed("data/small_trades.csv");

  ASSERT_EQ(feed.remaining(), kSmallTradeRows);

  size_t total = 0;
  me::TimestampNsT lastTs = 0;
  bool sawSnap = false, sawTrade = false;
  size_t snapCnt = 0, tradeCnt = 0;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    TypeMonitorStage<SparseCtx>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               TypeMonitorStage<SparseCtx>{total, lastTs, sawSnap, sawTrade,
                                           snapCnt, tradeCnt});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(total, kSmallTradeRows);
  EXPECT_TRUE(sawTrade);
  EXPECT_FALSE(sawSnap);
  EXPECT_EQ(tradeCnt, kSmallTradeRows);
  EXPECT_EQ(snapCnt, 0u);
  EXPECT_EQ(lastTs, 1722470403094140LL);

  EXPECT_FALSE(book.getBestBid().has_value());
  EXPECT_FALSE(book.getBestAsk().has_value());
}

TEST(EngineIntegration, MixedSourceOrderedAndUpdatesBook) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::MixedEventSource feed("data/small_lob.csv", "data/small_trades.csv");

  ASSERT_EQ(feed.remaining(), kSmallLobRows + kSmallTradeRows);

  size_t total = 0;
  me::TimestampNsT prev = std::numeric_limits<me::TimestampNsT>::min();
  me::TimestampNsT lastTs = 0;
  bool sawSnap = false, sawTrade = false;
  size_t snapCnt = 0, tradeCnt = 0;

  me::EventPipeline<SparseCtx, me::BookUpdateStage<me::SparseOrderBook>,
                    TypeMonitorStage<SparseCtx>, OrderCheckStage<SparseCtx>>
      pipeline(me::BookUpdateStage<me::SparseOrderBook>{},
               TypeMonitorStage<SparseCtx>{total, lastTs, sawSnap, sawTrade,
                                           snapCnt, tradeCnt},
               OrderCheckStage<SparseCtx>{prev});

  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_EQ(total, kSmallLobRows + kSmallTradeRows);
  EXPECT_TRUE(sawSnap);
  EXPECT_TRUE(sawTrade);
  EXPECT_EQ(snapCnt, kSmallLobRows);
  EXPECT_EQ(tradeCnt, kSmallTradeRows);

  auto bestBid = book.getBestBid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_DOUBLE_EQ(*bestBid, 0.0110263);
  auto bestAsk = book.getBestAsk();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_DOUBLE_EQ(*bestAsk, 0.0110264);
}