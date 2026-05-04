#pragma once

#include "MarketEngine/Common/Order.hpp"
#include "MarketEngine/OrderBook/LimitOrderBook.hpp"

namespace me {

// TODO: add constraits

struct AddOrderEvent {
  Order order;

  void visit(SparseOrderBook::BidBook &bids, SparseOrderBook::AskBook &asks);
  void visit(FullOrderBook::BidBook &bids, FullOrderBook::AskBook &asks);
};

struct CancelOrderEvent {
  OrderId id;

  void visit(SparseOrderBook::BidBook &bids, SparseOrderBook::AskBook &asks);
  void visit(FullOrderBook::BidBook &bids, FullOrderBook::AskBook &asks);
};

struct ModifyOrderEvent {
  OrderId id;
  QtyT newQty{};

  void visit(SparseOrderBook::BidBook &bids, SparseOrderBook::AskBook &asks);
  void visit(FullOrderBook::BidBook &bids, FullOrderBook::AskBook &asks);
};

struct ExecuteMarketEvent {
  QtyT qty{};
  Side side{};
  QtyT executedQty{};

  void visit(SparseOrderBook::BidBook &bids, SparseOrderBook::AskBook &asks);
  void visit(FullOrderBook::BidBook &bids, FullOrderBook::AskBook &asks);
};

struct SnapshotUpdateEvent {
  SparseOrderBook::BidBook newBids;
  SparseOrderBook::AskBook newAsks;

  void visit(SparseOrderBook::BidBook &bids, SparseOrderBook::AskBook &asks);
  void visit(FullOrderBook::BidBook &, FullOrderBook::AskBook &);
};

} // namespace me