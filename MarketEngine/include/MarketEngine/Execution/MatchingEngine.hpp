#pragma once

#include "MarketEngine/Execution/ActiveOrder.hpp"
#include "MarketEngine/Execution/ExecutionEvent.hpp"
#include "MarketEngine/Execution/ExecutionReport.hpp"
#include "MarketEngine/Execution/Fill.hpp"
#include "MarketEngine/OrderBook/OrderBookConcept.hpp"
#include "MarketEngine/Strategy/ModelEvent.hpp"

#include <span>

namespace me {

template <OrderBookConcept BookType> class MatchingEngine final {
public:
  using ActiveOrderMap = std::map<OrderId, ActiveOrder>;

  struct MatchResult final {
    std::vector<ExecutionEvent> events;
    bool empty() const { return events.empty(); }
  };

  MatchResult submit(const ModelEvent &request, const BookType &book,
                     TimestampNsT now = {}) {
    return process(std::span<const ModelEvent>(&request, 1), book, now);
  }

  MatchResult process(std::span<const ModelEvent> requests,
                      const BookType &book, TimestampNsT now = {}) {
    Liquidity liquidity = makeLiquidity(book);
    MatchResult result;

    for (const auto &request : requests) {
      std::visit(
          [&](const auto &value) {
            handleRequest(value, liquidity, result, now);
          },
          request);
    }

    matchActiveOrders(liquidity, result);
    return result;
  }

  MatchResult match(const BookType &book) {
    return process(std::span<const ModelEvent>{}, book);
  }

  const ActiveOrderMap &activeOrders() const { return activeOrders_; }

  const ActiveOrder *findActiveOrder(OrderId id) const {
    auto it = activeOrders_.find(id);
    if (it == activeOrders_.end()) {
      return nullptr;
    }
    return &it->second;
  }

private:
  struct TopOfBook final {
    PriceT price{};
    QtyT availableQty{};
  };

  struct Liquidity final {
    std::optional<TopOfBook> bid;
    std::optional<TopOfBook> ask;
  };

  ActiveOrderMap activeOrders_;

  template <typename LevelValueT>
  static QtyT levelQty(const LevelValueT &value) {
    if constexpr (std::is_arithmetic_v<LevelValueT>) {
      return static_cast<QtyT>(value);
    } else {
      QtyT qty{};
      for (const auto &order : value) {
        qty += order.qty;
      }
      return qty;
    }
  }

  template <typename IteratorT>
  static std::optional<TopOfBook> topOfBook(IteratorT begin, IteratorT end) {
    if (begin == end) {
      return std::nullopt;
    }

    return TopOfBook{
        begin->first,
        levelQty(begin->second),
    };
  }

  static Liquidity makeLiquidity(const BookType &book) {
    Liquidity liquidity;

    auto [bidBegin, bidEnd] = book.bids();
    liquidity.bid = topOfBook(bidBegin, bidEnd);

    auto [askBegin, askEnd] = book.asks();
    liquidity.ask = topOfBook(askBegin, askEnd);

    return liquidity;
  }

  static std::optional<TopOfBook> &oppositeLiquidity(Liquidity &liquidity,
                                                     Side side) {
    return side == Side::Buy ? liquidity.ask : liquidity.bid;
  }

  static const std::optional<TopOfBook> &
  oppositeLiquidity(const Liquidity &liquidity, Side side) {
    return side == Side::Buy ? liquidity.ask : liquidity.bid;
  }

  static bool crosses(const ActiveOrder &order, const Liquidity &liquidity) {
    const auto &opposite = oppositeLiquidity(liquidity, order.side);

    if (!opposite || opposite->availableQty <= QtyT{}) {
      return false;
    }

    return order.side == Side::Buy ? order.price >= opposite->price
                                   : order.price <= opposite->price;
  }

  static void emitFill(MatchResult &result, const Fill &fill) {
    result.events.emplace_back(fill);
  }

  static void emitReport(MatchResult &result, OrderId orderId,
                         ExecutionStatus status, QtyT remainingQty) {
    result.events.emplace_back(ExecutionReport{
        orderId,
        status,
        remainingQty,
    });
  }

  void tryFill(ActiveOrder &order, Liquidity &liquidity, MatchResult &result) {
    if (!crosses(order, liquidity)) {
      return;
    }

    auto &opposite = oppositeLiquidity(liquidity, order.side);
    if (!opposite || opposite->availableQty <= QtyT{}) {
      return;
    }

    const QtyT fillQty = std::min(order.remainingQty, opposite->availableQty);
    if (fillQty <= QtyT{}) {
      return;
    }

    emitFill(result, Fill{order.id, order.side, opposite->price, fillQty});

    order.remainingQty -= fillQty;
    opposite->availableQty -= fillQty;
  }

  void tryFillMarket(OrderId id, Side side, QtyT qty, Liquidity &liquidity,
                     MatchResult &result) {
    auto &opposite = oppositeLiquidity(liquidity, side);

    if (!opposite || opposite->availableQty <= QtyT{} || qty <= QtyT{}) {
      emitReport(result, id, ExecutionStatus::Rejected, qty);
      return;
    }

    const QtyT fillQty = std::min(qty, opposite->availableQty);

    if (fillQty > QtyT{}) {
      emitFill(result, Fill{id, side, opposite->price, fillQty});
      opposite->availableQty -= fillQty;
    }

    const QtyT remainingQty = qty - fillQty;
    emitReport(result, id,
               remainingQty > QtyT{} ? ExecutionStatus::PartiallyFilled
                                     : ExecutionStatus::Filled,
               remainingQty);
  }

  void handleRequest(const AddOrderRequest &request, Liquidity &liquidity,
                     MatchResult &result, TimestampNsT now) {
    const Order &order = request.order;

    if (order.id == 0 || order.qty <= QtyT{} ||
        activeOrders_.contains(order.id)) {
      emitReport(result, order.id, ExecutionStatus::Rejected, order.qty);
      return;
    }

    ActiveOrder active{
        order.id,
        order.side,
        order.price,
        order.qty,
        order.timestamp != 0 ? order.timestamp : now,
    };

    const QtyT initialQty = active.remainingQty;
    tryFill(active, liquidity, result);

    if (active.remainingQty > QtyT{}) {
      activeOrders_.emplace(active.id, active);

      emitReport(result, active.id,
                 active.remainingQty == initialQty
                     ? ExecutionStatus::Accepted
                     : ExecutionStatus::PartiallyFilled,
                 active.remainingQty);
      return;
    }

    emitReport(result, order.id, ExecutionStatus::Filled, QtyT{});
  }

  void handleRequest(const CancelOrderRequest &request, Liquidity &,
                     MatchResult &result, TimestampNsT) {
    const auto removed = activeOrders_.erase(request.id);

    emitReport(result, request.id,
               removed == 0 ? ExecutionStatus::Rejected
                            : ExecutionStatus::Canceled,
               QtyT{});
  }

  void handleRequest(const ModifyOrderRequest &request, Liquidity &liquidity,
                     MatchResult &result, TimestampNsT) {
    auto it = activeOrders_.find(request.id);
    if (it == activeOrders_.end()) {
      emitReport(result, request.id, ExecutionStatus::Rejected, request.newQty);
      return;
    }

    if (request.newQty <= QtyT{}) {
      activeOrders_.erase(it);
      emitReport(result, request.id, ExecutionStatus::Canceled, QtyT{});
      return;
    }

    ActiveOrder active = it->second;
    activeOrders_.erase(it);

    if (request.newPrice > 0.0) {
      active.price = request.newPrice;
    }

    const QtyT before = active.remainingQty;
    active.remainingQty = request.newQty;

    tryFill(active, liquidity, result);

    if (active.remainingQty > QtyT{}) {
      activeOrders_.emplace(active.id, active);

      emitReport(result, active.id,
                 active.remainingQty == before
                     ? ExecutionStatus::Modified
                     : ExecutionStatus::PartiallyFilled,
                 active.remainingQty);
      return;
    }

    emitReport(result, request.id, ExecutionStatus::Filled, QtyT{});
  }

  void handleRequest(const MarketOrderRequest &request, Liquidity &liquidity,
                     MatchResult &result, TimestampNsT) {
    tryFillMarket(request.id, request.side, request.qty, liquidity, result);
  }

  void matchActiveOrders(Liquidity &liquidity, MatchResult &result) {
    for (auto it = activeOrders_.begin(); it != activeOrders_.end();) {
      ActiveOrder active = it->second;
      const QtyT before = active.remainingQty;

      tryFill(active, liquidity, result);

      if (active.remainingQty == before) {
        ++it;
        continue;
      }

      if (active.remainingQty <= QtyT{}) {
        const OrderId id = it->first;
        it = activeOrders_.erase(it);

        emitReport(result, id, ExecutionStatus::Filled, QtyT{});
        continue;
      }

      it->second = active;
      emitReport(result, active.id, ExecutionStatus::PartiallyFilled,
                 active.remainingQty);
      ++it;
    }
  }
};

} // namespace me