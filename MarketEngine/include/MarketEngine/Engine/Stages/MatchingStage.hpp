#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"
#include "MarketEngine/Execution/MatchingEngine.hpp"

#include <utility>

namespace me {

template <OrderBookConcept BookType> class MatchingStage final {
public:
  void process(const MarketEvent &, EngineContext<BookType> &ctx) {
    auto result =
        engine_.process(ctx.pendingModelEvents.view(), ctx.book, ctx.now);
    ctx.pendingModelEvents.clear();

    for (auto &executionEvent : result.events) {
      ctx.executionEvents.push_back(std::move(executionEvent));
    }
  }

  const MatchingEngine<BookType> &engine() const { return engine_; }

private:
  MatchingEngine<BookType> engine_;
};

} // namespace me
