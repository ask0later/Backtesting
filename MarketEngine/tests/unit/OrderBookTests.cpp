#include "MarketEngine/Common/Order.hpp"
#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/OrderBook/BookEvent.hpp"
#include <gtest/gtest.h>

template <typename Iter> bool rangeEmpty(const std::pair<Iter, Iter> &range) {
  return range.first == range.second;
}

TEST(SparseOrderBook, EmptyBook) {
  me::SparseOrderBook book;
  EXPECT_FALSE(book.getBestBid().has_value());
  EXPECT_FALSE(book.getBestAsk().has_value());
  EXPECT_TRUE(rangeEmpty(book.bids()));
  EXPECT_TRUE(rangeEmpty(book.asks()));
}

TEST(SparseOrderBook, AddBid) {
  me::SparseOrderBook book;
  me::Order ord{1, 100.0, 10, me::Side::Buy, 0};
  me::AddOrderEvent add{ord};
  book.accept(add);

  ASSERT_TRUE(book.getBestBid().has_value());
  EXPECT_EQ(*book.getBestBid(), 100.0);
  EXPECT_FALSE(book.getBestAsk().has_value());

  auto bids = book.bids();
  ASSERT_FALSE(rangeEmpty(bids));
  EXPECT_EQ(bids.first->first, 100.0);
  EXPECT_EQ(bids.first->second, 10);
}

TEST(SparseOrderBook, AggregateSamePrice) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Buy, 0}});
  book.accept(me::AddOrderEvent{me::Order{2, 100.0, 20, me::Side::Buy, 0}});

  auto bids = book.bids();
  ASSERT_FALSE(rangeEmpty(bids));
  EXPECT_EQ(bids.first->second, 30);
}

TEST(SparseOrderBook, MarketBuy) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Sell, 0}});
  book.accept(me::AddOrderEvent{me::Order{2, 101.0, 20, me::Side::Sell, 0}});

  me::ExecuteMarketEvent exec{15, me::Side::Buy};
  book.accept(exec);
  EXPECT_EQ(exec.executedQty, 15);

  auto asks = book.asks();
  ASSERT_FALSE(rangeEmpty(asks));
  EXPECT_EQ(asks.first->first, 101.0);
  EXPECT_EQ(asks.first->second, 15);
}

TEST(SparseOrderBook, MarketSellPartial) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 99.0, 30, me::Side::Buy, 0}});

  me::ExecuteMarketEvent exec{10, me::Side::Sell};
  book.accept(exec);
  EXPECT_EQ(exec.executedQty, 10);
  EXPECT_EQ(book.bids().first->second, 20);
}

TEST(SparseOrderBook, MarketEatAll) {
  me::SparseOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 50.0, 10, me::Side::Buy, 0}});

  me::ExecuteMarketEvent exec{100, me::Side::Sell};
  book.accept(exec);
  EXPECT_EQ(exec.executedQty, 10);
  EXPECT_TRUE(rangeEmpty(book.bids()));
}

TEST(FullOrderBook, AddSingleOrder) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Buy, 1000}});

  ASSERT_TRUE(book.getBestBid().has_value());
  EXPECT_EQ(*book.getBestBid(), 100.0);

  auto bids = book.bids();
  const auto &orders = bids.first->second;
  ASSERT_EQ(orders.size(), 1);
  EXPECT_EQ(orders.begin()->id, 1);
  EXPECT_EQ(orders.begin()->qty, 10);
  EXPECT_EQ(orders.begin()->timestamp, 1000);
}

TEST(FullOrderBook, TimePriorityOrdering) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Buy, 2000}});
  book.accept(me::AddOrderEvent{me::Order{2, 100.0, 20, me::Side::Buy, 1000}});

  auto bids = book.bids();
  const auto &orders = bids.first->second;
  ASSERT_EQ(orders.size(), 2);
  auto it = orders.begin();
  EXPECT_EQ(it->id, 2);
  EXPECT_EQ(it->qty, 20);
  ++it;
  EXPECT_EQ(it->id, 1);
}

TEST(FullOrderBook, CancelExistingOrder) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Sell, 500}});
  ASSERT_FALSE(rangeEmpty(book.asks()));

  me::CancelOrderEvent cancel{1};
  book.accept(cancel);
  EXPECT_TRUE(rangeEmpty(book.asks()));
}

TEST(FullOrderBook, CancelRemovesLevelWhenEmpty) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 5, me::Side::Buy, 100}});
  book.accept(me::AddOrderEvent{me::Order{2, 100.0, 5, me::Side::Buy, 200}});
  ASSERT_EQ(book.bids().first->second.size(), 2);

  book.accept(me::CancelOrderEvent{1});
  ASSERT_EQ(book.bids().first->second.size(), 1);

  book.accept(me::CancelOrderEvent{2});
  EXPECT_TRUE(rangeEmpty(book.bids()));
}

TEST(FullOrderBook, CancelNonexistentDoesNothing) {
  me::FullOrderBook book;
  EXPECT_NO_THROW(book.accept(me::CancelOrderEvent{999}));
}

TEST(FullOrderBook, ModifyQty) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Sell, 300}});

  me::ModifyOrderEvent mod{1, 25};
  book.accept(mod);
  const auto &orders = book.asks().first->second;
  ASSERT_EQ(orders.size(), 1);
  EXPECT_EQ(orders.begin()->qty, 25);
  EXPECT_EQ(orders.begin()->timestamp, 300);
}

TEST(FullOrderBook, ModifyToZeroRemoves) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 10, me::Side::Buy, 0}});

  me::ModifyOrderEvent mod{1, 0};
  book.accept(mod);
  EXPECT_TRUE(rangeEmpty(book.bids()));
}

TEST(FullOrderBook, MarketBuyRespectsTimePriority) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 5, me::Side::Sell, 100}});
  book.accept(me::AddOrderEvent{me::Order{2, 100.0, 10, me::Side::Sell, 200}});
  book.accept(me::AddOrderEvent{me::Order{3, 101.0, 15, me::Side::Sell, 50}});

  me::ExecuteMarketEvent exec{12, me::Side::Buy};
  book.accept(exec);
  EXPECT_EQ(exec.executedQty, 12);

  auto asks = book.asks();
  ASSERT_FALSE(rangeEmpty(asks));
  auto levelIt = asks.first;
  EXPECT_EQ(levelIt->first, 100.0);
  EXPECT_EQ(levelIt->second.size(), 1);
  EXPECT_EQ(levelIt->second.begin()->id, 2);
  EXPECT_EQ(levelIt->second.begin()->qty, 3);

  ++levelIt;
  ASSERT_NE(levelIt, asks.second);
  EXPECT_EQ(levelIt->first, 101.0);
  EXPECT_EQ(levelIt->second.size(), 1);
}

TEST(FullOrderBook, MarketSellMultipleLevels) {
  me::FullOrderBook book;
  book.accept(me::AddOrderEvent{me::Order{1, 100.0, 5, me::Side::Buy, 100}});
  book.accept(me::AddOrderEvent{me::Order{2, 99.0, 10, me::Side::Buy, 200}});

  me::ExecuteMarketEvent exec{8, me::Side::Sell};
  book.accept(exec);
  EXPECT_EQ(exec.executedQty, 8);

  auto bids = book.bids();
  ASSERT_FALSE(rangeEmpty(bids));
  EXPECT_EQ(bids.first->first, 99.0);
  const auto &orders = bids.first->second;
  EXPECT_EQ(orders.size(), 1);
  EXPECT_EQ(orders.begin()->qty, 7);
}