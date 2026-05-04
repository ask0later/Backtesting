#pragma once

#include "MarketEngine/Common/Types.hpp"

namespace me {

using OrderId = uint64_t;

struct Order final {
  OrderId id;
  PriceT price;
  QtyT qty;
  Side side;
  TimestampNsT timestamp;

  auto operator<=>(const Order &other) const {
    if (auto t = timestamp <=> other.timestamp; t != 0)
      return t;
    return id <=> other.id;
  }
};

} // namespace me