#include <gtest/gtest.h>

#include "MarketEngine/Engine/Engine.hpp"
#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Engine/EventPipeline.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"

namespace {

void applyToSparseBook(me::SparseOrderBook &book,
                       const me::MarketEvent &event) {
  auto &mutableEvent = const_cast<me::MarketEventType &>(event.event);
  std::visit([&book](auto &visitor) { book.accept(visitor); }, mutableEvent);
}

void applyToFullBook(me::FullOrderBook &book, const me::MarketEvent &event) {
  auto &mutableEvent = const_cast<me::MarketEventType &>(event.event);
  std::visit([&book](auto &visitor) { book.accept(visitor); }, mutableEvent);
}

using SparseCtx = me::EngineContext<me::SparseOrderBook>;
using FullCtx = me::EngineContext<me::FullOrderBook>;

struct CountStage {
  int &count;
  void process(const me::MarketEvent &, SparseCtx &) { ++count; }
  void process(const me::MarketEvent &, FullCtx &) { ++count; }
};

struct TimestampStage {
  me::TimestampNsT &ts;
  void process(const me::MarketEvent &, SparseCtx &ctx) { ts = ctx.now; }
  void process(const me::MarketEvent &, FullCtx &ctx) { ts = ctx.now; }
};

struct CurrentEventStage {
  me::TimestampNsT &timestamp;
  void process(const me::MarketEvent &event, SparseCtx &) {
    timestamp = event.timestamp;
  }
  void process(const me::MarketEvent &event, FullCtx &) {
    timestamp = event.timestamp;
  }
};

struct TraceStage {
  std::string &trace;
  std::string tag;
  void process(const me::MarketEvent &, SparseCtx &) { trace += tag; }
  void process(const me::MarketEvent &, FullCtx &) { trace += tag; }
};

struct ApplyToSparseBookStage {
  me::SparseOrderBook &book;
  void process(const me::MarketEvent &e, SparseCtx &) {
    applyToSparseBook(book, e);
  }
};

struct ApplyToFullBookStage {
  me::FullOrderBook &book;
  void process(const me::MarketEvent &e, FullCtx &) {
    applyToFullBook(book, e);
  }
};

struct VectorFeed : me::MarketEventSource<VectorFeed> {
  explicit VectorFeed(std::vector<me::MarketEvent> events)
      : MarketEventSource(std::move(events)) {}
};

} // namespace

TEST(EngineTest, ProcessesAllEvents) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  VectorFeed feed({{100, me::AddOrderEvent{}}, {200, me::CancelOrderEvent{}}});
  int calls = 0;
  me::EventPipeline<SparseCtx, CountStage> pipeline(CountStage{calls});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  EXPECT_EQ(calls, 2);
}

TEST(EngineTest, EmptyFeed) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  VectorFeed feed({});
  int calls = 0;
  me::EventPipeline<SparseCtx, CountStage> pipeline(CountStage{calls});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  EXPECT_EQ(calls, 0);
}

TEST(EngineTest, ContextTimestampUpdated) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  VectorFeed feed({{42, me::AddOrderEvent{}}});
  me::TimestampNsT captured = 0;
  me::EventPipeline<SparseCtx, TimestampStage> pipeline(
      TimestampStage{captured});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  EXPECT_EQ(captured, 42);
}

TEST(EngineTest, CurrentEventPointerSet) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  me::MarketEvent ev{10, me::AddOrderEvent{}};
  VectorFeed feed({ev});
  me::TimestampNsT timestamp = 0;
  me::EventPipeline<SparseCtx, CurrentEventStage> pipeline(
      CurrentEventStage{timestamp});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  EXPECT_EQ(timestamp, 10);
}

TEST(EngineTest, PipelineOrder) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};
  VectorFeed feed({{0, me::AddOrderEvent{}}});
  std::string trace;
  me::EventPipeline<SparseCtx, TraceStage, TraceStage, TraceStage> pipeline(
      TraceStage{trace, "A"}, TraceStage{trace, "B"}, TraceStage{trace, "C"});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();
  EXPECT_EQ(trace, "ABC");
}

TEST(EngineTest, AddOrderUpdatesBook) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::Order buy{1, 100.0, 30.0, me::Side::Buy};
  me::MarketEvent ev{1000, me::AddOrderEvent{buy}};
  VectorFeed feed({ev});

  me::EventPipeline<SparseCtx, ApplyToSparseBookStage> pipeline(
      ApplyToSparseBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto bestBid = book.getBestBid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_DOUBLE_EQ(*bestBid, 100.0);
  EXPECT_DOUBLE_EQ(book.bids().first->second, 30.0);
}

TEST(EngineTest, ExecuteMarketReducesLiquidity) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::Order buy{1, 100.0, 100.0, me::Side::Buy};
  me::Order sell{2, 110.0, 200.0, me::Side::Sell};
  me::ExecuteMarketEvent market{30, me::Side::Buy, 0};

  VectorFeed feed(
      {{1, me::AddOrderEvent{buy}}, {2, me::AddOrderEvent{sell}}, {3, market}});

  me::EventPipeline<SparseCtx, ApplyToSparseBookStage> pipeline(
      ApplyToSparseBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto bestAsk = book.getBestAsk();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_DOUBLE_EQ(*bestAsk, 110.0);
  EXPECT_DOUBLE_EQ(book.asks().first->second, 170.0);
}

TEST(EngineTest, SnapshotReplacesBook) {
  me::SparseOrderBook book;
  SparseCtx ctx{.book = book};

  me::Order buy{1, 100.0, 10.0, me::Side::Buy};
  me::SnapshotUpdateEvent snap;
  snap.newBids = {{200.0, 50.0}};
  snap.newAsks = {{210.0, 30.0}};

  VectorFeed feed({{1, me::AddOrderEvent{buy}}, {2, snap}});

  me::EventPipeline<SparseCtx, ApplyToSparseBookStage> pipeline(
      ApplyToSparseBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto bestBid = book.getBestBid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_DOUBLE_EQ(*bestBid, 200.0);
  EXPECT_DOUBLE_EQ(book.bids().first->second, 50.0);
  auto bestAsk = book.getBestAsk();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_DOUBLE_EQ(*bestAsk, 210.0);
}

TEST(FullOrderBookTest, AddOrder) {
  me::FullOrderBook book;
  FullCtx ctx{.book = book};

  me::Order buy{1, 100.0, 50.0, me::Side::Buy};
  VectorFeed feed({{1, me::AddOrderEvent{buy}}});

  me::EventPipeline<FullCtx, ApplyToFullBookStage> pipeline(
      ApplyToFullBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto [begin, end] = book.bids();
  ASSERT_NE(begin, end);
  EXPECT_DOUBLE_EQ(begin->first, 100.0);
  const auto &orders = begin->second;
  ASSERT_EQ(orders.size(), 1u);
  EXPECT_EQ(orders.begin()->id, 1);
  EXPECT_DOUBLE_EQ(orders.begin()->qty, 50.0);
}

TEST(FullOrderBookTest, CancelOrder) {
  me::FullOrderBook book;
  FullCtx ctx{.book = book};

  me::Order buy{1, 100.0, 50.0, me::Side::Buy};
  me::MarketEvent addEv{1, me::AddOrderEvent{buy}};
  me::MarketEvent cancelEv{2, me::CancelOrderEvent{1}};

  VectorFeed feed({addEv, cancelEv});
  me::EventPipeline<FullCtx, ApplyToFullBookStage> pipeline(
      ApplyToFullBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  EXPECT_FALSE(book.getBestBid().has_value());
  EXPECT_EQ(book.bids().first, book.bids().second);
}

TEST(FullOrderBookTest, ModifyOrderChangesQty) {
  me::FullOrderBook book;
  FullCtx ctx{.book = book};

  me::Order buy{1, 100.0, 50.0, me::Side::Buy};
  me::MarketEvent addEv{1, me::AddOrderEvent{buy}};
  me::MarketEvent modEv{2, me::ModifyOrderEvent{1, 77.0}};

  VectorFeed feed({addEv, modEv});
  me::EventPipeline<FullCtx, ApplyToFullBookStage> pipeline(
      ApplyToFullBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto bestBid = book.getBestBid();
  ASSERT_TRUE(bestBid.has_value());
  EXPECT_DOUBLE_EQ(*bestBid, 100.0);

  auto [begin, end] = book.bids();
  ASSERT_NE(begin, end);
  const auto &orders = begin->second;
  ASSERT_EQ(orders.size(), 1u);
  EXPECT_DOUBLE_EQ(orders.begin()->qty, 77.0);
}

TEST(FullOrderBookTest, ExecuteMarketReducesOrders) {
  me::FullOrderBook book;
  FullCtx ctx{.book = book};

  me::Order ask1{10, 110.0, 100.0, me::Side::Sell};
  me::Order ask2{11, 110.0, 150.0, me::Side::Sell};
  me::ExecuteMarketEvent marketBuy{120, me::Side::Buy, 0};

  VectorFeed feed({{1, me::AddOrderEvent{ask1}},
                   {2, me::AddOrderEvent{ask2}},
                   {3, marketBuy}});

  me::EventPipeline<FullCtx, ApplyToFullBookStage> pipeline(
      ApplyToFullBookStage{book});
  me::Engine engine(std::move(feed), std::move(pipeline), std::move(ctx));
  engine.run();

  auto bestAsk = book.getBestAsk();
  ASSERT_TRUE(bestAsk.has_value());
  EXPECT_DOUBLE_EQ(*bestAsk, 110.0);

  auto [begin, end] = book.asks();
  ASSERT_NE(begin, end);
  const auto &orders = begin->second;
  ASSERT_EQ(orders.size(), 1u);
  EXPECT_EQ(orders.begin()->id, 11);
  EXPECT_DOUBLE_EQ(orders.begin()->qty, 130.0);
}