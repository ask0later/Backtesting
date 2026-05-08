#pragma once

#include "MarketEngine/Common/Order.hpp"

#include <variant>

namespace me {

struct AddOrderRequest final {
  Order order;
};

struct CancelOrderRequest final {
  OrderId id{};
};

struct ModifyOrderRequest final {
  OrderId id{};
  QtyT newQty{};
};

using OrderRequest =
    std::variant<AddOrderRequest, CancelOrderRequest, ModifyOrderRequest>;

} // namespace me