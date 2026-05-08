#pragma once

#include "MarketEngine/MarketDataFeed/EventSource.hpp"
#include "MarketEngine/Strategy/StrategyContext.hpp"

namespace me {

template <OrderBookConcept BookType> class StrategyAdapter {
public:
  virtual ~StrategyAdapter() = default;

  virtual void onMarketEvent(const Event &event,
                             StrategyContext<BookType> &ctx) = 0;
};

} // namespace me