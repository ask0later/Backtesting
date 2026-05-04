#pragma once

#include "MarketEngine/Common/Order.hpp"
#include "MarketEngine/Common/Types.hpp"
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace me {

template <typename ConcreteBook, typename KeyT, typename ValueT>
class OrderBook {
public:
  using LevelKeyT = KeyT;
  using LevelValueT = ValueT;
  using BidBook = std::map<LevelKeyT, LevelValueT, std::greater<>>;
  using AskBook = std::map<LevelKeyT, LevelValueT, std::less<>>;
  using BidIterator = typename BidBook::const_iterator;
  using AskIterator = typename AskBook::const_iterator;

private:
  BidBook bids_;
  AskBook asks_;

public:
  std::optional<LevelKeyT> getBestBid() const {
    return bids_.empty() ? std::nullopt : std::optional(bids_.begin()->first);
  }

  std::optional<LevelKeyT> getBestAsk() const {
    return asks_.empty() ? std::nullopt : std::optional(asks_.begin()->first);
  }

  std::pair<BidIterator, BidIterator> bids() const {
    return {bids_.cbegin(), bids_.cend()};
  }

  std::pair<AskIterator, AskIterator> asks() const {
    return {asks_.cbegin(), asks_.cend()};
  }

  template <typename BookVisitor> void accept(BookVisitor &&visitor) {
    std::forward<BookVisitor &&>(visitor).visit(bids_, asks_);
  }
};

// Level 2 Order book (price and volume)
class SparseOrderBook final : public OrderBook<SparseOrderBook, PriceT, QtyT> {
};

// Level 3 Order book (order-by-order)
class FullOrderBook final
    : public OrderBook<FullOrderBook, PriceT, std::set<Order>> {};

} // namespace me