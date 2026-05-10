#pragma once

#include "MarketEngine/Execution/ExecutionReport.hpp"
#include "MarketEngine/Execution/Fill.hpp"

#include <variant>

namespace me {

using ExecutionEvent = std::variant<Fill, ExecutionReport>;

} // namespace me