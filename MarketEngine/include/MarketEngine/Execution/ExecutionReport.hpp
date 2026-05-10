#pragma once

#include "MarketEngine/Common/Order.hpp"

namespace me {

enum class ExecutionStatus {
  Accepted,
  Modified,
  Canceled,
  PartiallyFilled,
  Filled,
  Rejected
};

struct ExecutionReport final {
  OrderId orderId{};
  ExecutionStatus status{ExecutionStatus::Accepted};
  QtyT remainingQty{};
};

} // namespace me
