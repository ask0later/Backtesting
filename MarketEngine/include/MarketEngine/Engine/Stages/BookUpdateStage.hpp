#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"
#include <variant>

namespace me {

template <OrderBookConcept BookType> class BookUpdateStage final {
public:
  void process(const MarketEvent &event, EngineContext<BookType> &ctx) {
    MarketEventType tmpEvent = event.event;
    std::visit([&](auto &bookEvent) { ctx.book.accept(bookEvent); }, tmpEvent);
  }
};

} // namespace me