#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Strategy/StrategyAdapter.hpp"
#include "MarketEngine/Strategy/StrategyContext.hpp"

#include <utility>

namespace me {

template <OrderBookConcept BookType> class StrategyStage final {
public:
  explicit StrategyStage(StrategyAdapter<BookType> &strategy)
      : strategy_(strategy) {}

  void process(const Event &event, EngineContext<BookType> &ctx) {
    StrategyContext<BookType> strategyCtx(ctx.book);

    strategy_.onMarketEvent(event, strategyCtx);

    for (auto &request : strategyCtx.drainRequests()) {
      ctx.orderIngress.push(std::move(request));
    }
  }

private:
  StrategyAdapter<BookType> &strategy_;
};

} // namespace me