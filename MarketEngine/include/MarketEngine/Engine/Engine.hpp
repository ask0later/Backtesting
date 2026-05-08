#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"

#include <concepts>
#include <optional>

namespace me {

template <typename FeedType>
concept EventFeedConcept = requires(FeedType feed) {
  { feed.nextEvent() } -> std::same_as<std::optional<Event>>;
};

template <typename PipelineType, typename ContextType>
concept EventPipelineConcept =
    requires(PipelineType pipeline, const Event &event, ContextType &ctx) {
      { pipeline.process(event, ctx) } -> std::same_as<void>;
    };

template <EventFeedConcept FeedType, typename PipelineType,
          OrderBookConcept BookType>
  requires EventPipelineConcept<PipelineType, EngineContext<BookType>>
class Engine final {
public:
  using Context = EngineContext<BookType>;

  Engine(FeedType feed, PipelineType pipeline, Context ctx)
      : feed_(std::move(feed)), pipeline_(std::move(pipeline)),
        ctx_(std::move(ctx)) {}

  void run() {
    while (auto &&event = feed_.nextEvent()) {
      ctx_.now = event->timestamp;
      pipeline_.process(*event, ctx_);
    }
  }

  const Context &context() const { return ctx_; }

private:
  FeedType feed_;
  PipelineType pipeline_;
  Context ctx_;
};

} // namespace me