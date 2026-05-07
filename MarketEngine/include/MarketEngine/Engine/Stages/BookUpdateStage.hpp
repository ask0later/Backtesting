#pragma once

#include "MarketEngine/Engine/EngineContext.hpp"
#include <variant>

namespace me {

template <OrderBookConcept BookType> class BookUpdateStage final {
public:
  void process(const Event &event, EngineContext<BookType> &ctx) {
    std::visit([&](auto &bookEvent) { ctx.book.accept(bookEvent); },
               event.event);
  }
};

} // namespace me