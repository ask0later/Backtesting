#pragma once

#include "MarketEngine/Common/Order.hpp"

namespace me {

struct Fill final {
  OrderId orderId{};
  Side side{};
  PriceT price{};
  QtyT qty{};
};

} // namespace me