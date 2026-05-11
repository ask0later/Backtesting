#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Metrics/MetricsEngine.hpp"
#include "MarketEngine/OrderBook/OrderBookConcept.hpp"

namespace me {

template <OrderBookConcept BookType> class MetricsStage final {
public:
  explicit MetricsStage(MetricsEngine &engine) : engine_(engine) {}

  void process(const MarketEvent &, EngineContext<BookType> &ctx) {
    for (; processedExecutionEvents_ < ctx.executionEvents.size();
         ++processedExecutionEvents_) {
      engine_.observe(ctx.executionEvents[processedExecutionEvents_]);
    }
  }

  size_t processedExecutionEvents() const { return processedExecutionEvents_; }

private:
  MetricsEngine &engine_;
  size_t processedExecutionEvents_{};
};

} // namespace me