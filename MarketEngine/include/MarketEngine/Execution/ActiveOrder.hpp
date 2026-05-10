#pragma once

#include "MarketEngine/Common/Order.hpp"

namespace me {

struct ActiveOrder final {
  OrderId id{};
  Side side{};
  PriceT price{};
  QtyT remainingQty{};
  TimestampNsT timestamp{};
};

} // namespace me