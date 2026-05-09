#pragma once

#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/MarketDataFeed/EventSource.hpp"
#include "MarketEngine/OrderBook/OrderBookConcept.hpp"
#include "MarketEngine/Strategy/ModelEvent.hpp"

namespace me {

template <OrderBookConcept BookType> struct EngineContext final {
  TimestampNsT now{};
  ModelEventBuffer pendingModelEvents;
  BookType &book;
};

} // namespace me