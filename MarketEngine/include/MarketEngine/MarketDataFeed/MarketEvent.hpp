#pragma once

#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/OrderBook/BookEvent.hpp"

#include <variant>

namespace me {

// Historical market data event. These events are the only events that mutate
// the historical order book through BookUpdateStage.
using MarketEventType =
    std::variant<AddOrderEvent, CancelOrderEvent, ModifyOrderEvent,
                 ExecuteMarketEvent, SnapshotUpdateEvent>;

struct MarketEvent final {
  TimestampNsT timestamp{};
  MarketEventType event;
};

} // namespace me
