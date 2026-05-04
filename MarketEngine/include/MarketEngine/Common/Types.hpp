#pragma once

#include <cstdint>
#include <string>

namespace me {

using PriceT = double;
using QtyT = double;
using TimestampNsT = int64_t;

enum class Side : uint8_t { Buy = 0, Sell = 1 };

} // namespace me