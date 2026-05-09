#pragma once

#include "MarketEngine/MarketDataFeed/MarketEvent.hpp"
#include "MarketEngine/Strategy/StrategyContext.hpp"

namespace me {

template <OrderBookConcept BookType> class StrategyAdapter {
public:
  virtual ~StrategyAdapter() = default;

  virtual void onMarketEvent(const MarketEvent &event,
                             StrategyContext<BookType> &ctx) = 0;
};

} // namespace me