#pragma once

#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/MarketDataFeed/MarketEvent.hpp"
#include "MarketEngine/Strategy/ModelEvent.hpp"

#include <concepts>
#include <queue>
#include <utility>

namespace me {

template <typename BookType>
concept OrderBookConcept = requires(BookType book) {
  typename BookType::LevelKeyT;
  typename BookType::LevelValueT;
  typename BookType::BidIterator;
  typename BookType::AskIterator;
  typename BookType::BidBook;
  typename BookType::AskBook;

  {
    book.bids()
  } -> std::same_as<std::pair<typename BookType::BidIterator,
                              typename BookType::BidIterator>>;
  {
    book.asks()
  } -> std::same_as<std::pair<typename BookType::AskIterator,
                              typename BookType::AskIterator>>;
};

template <OrderBookConcept BookType> struct EngineContext final {
  TimestampNsT now{};
  ModelEventBuffer pendingModelEvents;
  BookType &book;
};

} // namespace me