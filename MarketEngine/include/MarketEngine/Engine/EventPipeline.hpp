#pragma once

#include "MarketEngine/MarketDataFeed/MarketEvent.hpp"

#include <concepts>
#include <tuple>
#include <utility>

namespace me {

template <typename StageType, typename ContextType>
concept PipelineStageConcept =
    requires(StageType stage, const MarketEvent &event, ContextType &ctx) {
      { stage.process(event, ctx) } -> std::same_as<void>;
    };

// Pipeline that is executed after an event occurs
template <typename ContextType, typename... StagesTypes>
  requires(PipelineStageConcept<StagesTypes, ContextType> && ...)
class EventPipeline final {
public:
  explicit EventPipeline(StagesTypes... stages)
      : stages_(std::move(stages)...) {}

  void process(const MarketEvent &event, ContextType &ctx) {
    std::apply([&](auto &...stage) { (stage.process(event, ctx), ...); },
               stages_);
  }

private:
  std::tuple<StagesTypes...> stages_;
};

} // namespace me