#include "MarketEngine/MarketDataFeed/EventSource.hpp"

#include <fstream>
#include <limits>
#include <variant>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

constexpr int kSmallLobDataRows = 25;
constexpr int kSmallTradeDataRows = 29;

fs::path temp_csv(const fs::path &stem) {
  return fs::temp_directory_path() / stem;
}

} // namespace

TEST(LobSnapshotEventSource, FirstRowSnapshotMatchesCsvTopOfBook) {
  me::LobSnapshotEventSource src("data/small_lob.csv");

  ASSERT_EQ(src.remaining(), kSmallLobDataRows);

  auto ev = src.nextEvent();
  ASSERT_TRUE(ev.has_value());
  ASSERT_EQ(ev->timestamp, 1722470402038431LL);

  auto *snap = std::get_if<me::SnapshotUpdateEvent>(&ev->event);
  ASSERT_NE(snap, nullptr);
  ASSERT_FALSE(snap->newBids.empty());
  ASSERT_FALSE(snap->newAsks.empty());

  EXPECT_DOUBLE_EQ(snap->newBids.begin()->first, 0.0110435);
  EXPECT_DOUBLE_EQ(snap->newBids.begin()->second, 103687.0);
  EXPECT_DOUBLE_EQ(snap->newAsks.begin()->first, 0.0110436);
  EXPECT_DOUBLE_EQ(snap->newAsks.begin()->second, 121492.0);
}

TEST(LobSnapshotEventSource, IteratesOneEventPerCsvRowThenStops) {
  me::LobSnapshotEventSource src("data/small_lob.csv");

  for (int i = 0; i < kSmallLobDataRows; ++i) {
    ASSERT_TRUE(src.nextEvent().has_value()) << "row index " << i;
  }
  EXPECT_FALSE(src.nextEvent().has_value());
  EXPECT_EQ(src.remaining(), 0u);
}

TEST(LobSnapshotEventSource, ResetReplaysFromStart) {
  me::LobSnapshotEventSource src("data/small_lob.csv");

  ASSERT_TRUE(src.nextEvent().has_value());
  ASSERT_TRUE(src.nextEvent().has_value());
  src.reset();
  ASSERT_EQ(src.remaining(), kSmallLobDataRows);

  auto ev = src.nextEvent();
  ASSERT_TRUE(ev.has_value());
  EXPECT_EQ(ev->timestamp, 1722470402038431LL);
}

TEST(TradesEventSource, FirstRowIsSellAggressor) {
  me::TradesEventSource src("data/small_trades.csv");

  ASSERT_EQ(src.remaining(), kSmallTradeDataRows);

  auto ev = src.nextEvent();
  ASSERT_TRUE(ev.has_value());
  EXPECT_EQ(ev->timestamp, 1722470400014926LL);

  auto *ex = std::get_if<me::ExecuteMarketEvent>(&ev->event);
  ASSERT_NE(ex, nullptr);
  EXPECT_EQ(ex->side, me::Side::Sell);
  EXPECT_NEAR(ex->qty, 734.0, 1e-9);
}

TEST(TradesEventSource, FifthDataRowIsAggressiveBuyAfterFourSells) {
  me::TradesEventSource src("data/small_trades.csv");

  ASSERT_TRUE(src.nextEvent().has_value());
  ASSERT_TRUE(src.nextEvent().has_value());
  ASSERT_TRUE(src.nextEvent().has_value());

  auto sellThird = src.nextEvent();
  ASSERT_TRUE(sellThird.has_value());

  auto *sell = std::get_if<me::ExecuteMarketEvent>(&sellThird->event);
  ASSERT_NE(sell, nullptr);
  EXPECT_EQ(sell->side, me::Side::Sell);

  auto buyFourth = src.nextEvent();
  ASSERT_TRUE(buyFourth.has_value());
  EXPECT_EQ(buyFourth->timestamp, 1722470403047136LL);

  auto *buy = std::get_if<me::ExecuteMarketEvent>(&buyFourth->event);
  ASSERT_NE(buy, nullptr);
  EXPECT_EQ(buy->side, me::Side::Buy);
  EXPECT_NEAR(buy->qty, 5378.0, 1e-9);
}

TEST(TradesEventSource, UppercaseSideAliases) {
  const fs::path p = temp_csv("me_trades_side_aliases.csv");
  {
    std::ofstream o(p);
    o << "local_timestamp,side,price,amount\n";
    o << "1,B,1.0,2\n";
    o << "2,S,1.0,3\n";
  }

  me::TradesEventSource src(p);

  auto a = src.nextEvent();
  ASSERT_TRUE(a.has_value());
  EXPECT_EQ(std::get<me::ExecuteMarketEvent>(a->event).side, me::Side::Buy);

  auto b = src.nextEvent();
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(std::get<me::ExecuteMarketEvent>(b->event).side, me::Side::Sell);

  fs::remove(p);
}

TEST(MixedEventSource, MergesLobAndTradesByNonDecreasingTimestamp) {
  me::MixedEventSource src("data/small_lob.csv", "data/small_trades.csv");

  ASSERT_EQ(src.remaining(), kSmallLobDataRows + kSmallTradeDataRows);

  me::TimestampNsT prev = std::numeric_limits<me::TimestampNsT>::min();
  bool saw_snapshot = false;
  bool saw_trade = false;

  while (auto e = src.nextEvent()) {
    EXPECT_GE(e->timestamp, prev);
    prev = e->timestamp;
    std::visit(
        [&](auto &&v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, me::SnapshotUpdateEvent>) {
            saw_snapshot = true;
          } else if constexpr (std::is_same_v<T, me::ExecuteMarketEvent>) {
            saw_trade = true;
          }
        },
        e->event);
  }

  EXPECT_TRUE(saw_snapshot);
  EXPECT_TRUE(saw_trade);
}

TEST(MixedEventSource, LeadingTradesComeBeforeFirstLobWhenEarlier) {
  me::MixedEventSource src("data/small_lob.csv", "data/small_trades.csv");

  auto a = src.nextEvent();
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(std::holds_alternative<me::ExecuteMarketEvent>(a->event));
  EXPECT_EQ(a->timestamp, 1722470400014926LL);

  auto b = src.nextEvent();
  ASSERT_TRUE(b.has_value());
  ASSERT_TRUE(std::holds_alternative<me::SnapshotUpdateEvent>(b->event));
  EXPECT_EQ(b->timestamp, 1722470402038431LL);
}

TEST(TradesEventSource, MissingRequiredTradeColumnsThrowsWhenUsingLobCsv) {
  EXPECT_THROW(me::TradesEventSource("data/small_lob.csv"), std::runtime_error);
}