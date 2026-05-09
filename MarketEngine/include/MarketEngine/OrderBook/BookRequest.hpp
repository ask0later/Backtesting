#pragma once

#include "MarketEngine/Common/Order.hpp"

namespace me {

// Participant-generated requests addressed to the book model.

struct AddOrderRequest final {
  Order order;
};

struct CancelOrderRequest final {
  OrderId id{};
};

struct ModifyOrderRequest final {
  OrderId id{};
  QtyT newQty{};
  PriceT newPrice{};
};

struct MarketOrderRequest final {
  OrderId id{};
  Side side{};
  QtyT qty{};
};

} // namespace me
