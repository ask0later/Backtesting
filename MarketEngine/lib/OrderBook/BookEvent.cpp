#include "MarketEngine/OrderBook/BookEvent.hpp"
#include <algorithm>

namespace me {

void AddOrderEvent::visit(SparseOrderBook::BidBook &bids,
                          SparseOrderBook::AskBook &asks) {
  auto &&addTo = [order = order](auto &target) {
    const PriceT price = order.price;
    auto it = target.find(price);
    if (it != target.end()) {
      it->second += order.qty;
    } else {
      target[price] = order.qty;
    }
  };

  if (order.side == Side::Buy)
    addTo(bids);
  else
    addTo(asks);
}

void AddOrderEvent::visit(FullOrderBook::BidBook &bids,
                          FullOrderBook::AskBook &asks) {
  auto &&addTo = [order = order](auto &target) {
    target[order.price].insert(order);
  };

  if (order.side == Side::Buy)
    addTo(bids);
  else
    addTo(asks);
}

void CancelOrderEvent::visit(SparseOrderBook::BidBook &bids,
                             SparseOrderBook::AskBook &asks) {}

void CancelOrderEvent::visit(FullOrderBook::BidBook &bids,
                             FullOrderBook::AskBook &asks) {
  auto &&tryCancel = [id = id](auto &target) {
    for (auto levelIt = target.begin(); levelIt != target.end(); ++levelIt) {
      auto &orders = levelIt->second;
      auto it = std::find_if(orders.begin(), orders.end(),
                             [id = id](const Order &o) { return o.id == id; });
      if (it != orders.end()) {
        orders.erase(it);
        if (orders.empty()) {
          target.erase(levelIt);
        }
        return true;
      }
    }
    return false;
  };

  if (!tryCancel(bids)) {
    tryCancel(asks);
  }
}

void ModifyOrderEvent::visit(SparseOrderBook::BidBook &bids,
                             SparseOrderBook::AskBook &asks) {}

void ModifyOrderEvent::visit(FullOrderBook::BidBook &bids,
                             FullOrderBook::AskBook &asks) {
  auto &&tryModify = [id = id, newQty = newQty](auto &bookMap) {
    for (auto levelIt = bookMap.begin(); levelIt != bookMap.end(); ++levelIt) {
      auto &orders = levelIt->second;
      auto it = std::find_if(orders.begin(), orders.end(),
                             [id = id](const Order &o) { return o.id == id; });
      if (it != orders.end()) {
        if (newQty <= 0) {
          orders.erase(it);
        } else {
          Order updated = *it;
          updated.qty = newQty;
          orders.erase(it);
          orders.insert(updated);
        }
        if (orders.empty()) {
          bookMap.erase(levelIt);
        }
        return true;
      }
    }
    return false;
  };

  if (!tryModify(bids)) {
    tryModify(asks);
  }
}

void ExecuteMarketEvent::visit(SparseOrderBook::BidBook &bids,
                               SparseOrderBook::AskBook &asks) {
  auto &&execute = [qty = qty, side = side,
                    executedQty = &executedQty](auto &target) {
    QtyT remaining = qty;
    *executedQty = 0;

    auto it = target.begin();
    while (it != target.end() && remaining > 0) {
      if (it->second <= remaining) {
        remaining -= it->second;
        *executedQty += it->second;
        it = target.erase(it);
      } else {
        it->second -= remaining;
        *executedQty += remaining;
        remaining = 0;
        ++it;
      }
    }
  };

  if (side == Side::Buy)
    execute(asks);
  else
    execute(bids);
}

void ExecuteMarketEvent::visit(FullOrderBook::BidBook &bids,
                               FullOrderBook::AskBook &asks) {
  auto execute = [qty = qty, side = side,
                  executedQty = &executedQty](auto &target) {
    QtyT remaining = qty;
    *executedQty = 0;
    auto level_it = target.begin();
    while (level_it != target.end() && remaining > 0) {
      auto &orders = level_it->second;
      auto order_it = orders.begin();
      while (order_it != orders.end() && remaining > 0) {
        if (order_it->qty <= remaining) {
          remaining -= order_it->qty;
          *executedQty += order_it->qty;
          order_it = orders.erase(order_it);
        } else {
          Order updated = *order_it;
          updated.qty -= remaining;
          *executedQty += remaining;
          remaining = 0;
          orders.erase(order_it);
          orders.insert(updated);
          break;
        }
      }
      if (orders.empty())
        level_it = target.erase(level_it);
      else
        ++level_it;
    }
  };

  if (side == Side::Buy)
    execute(asks);
  else
    execute(bids);
}

void SnapshotUpdateEvent::visit(SparseOrderBook::BidBook &bids,
                                SparseOrderBook::AskBook &asks) {
  bids = std::move(newBids);
  asks = std::move(newAsks);
}

void SnapshotUpdateEvent::visit(FullOrderBook::BidBook &bids,
                                FullOrderBook::AskBook &asks) {}

} // namespace me