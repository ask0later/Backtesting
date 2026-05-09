#pragma once

#include <concepts>
#include <optional>
#include <utility>

namespace me {

template <typename BookType>
concept OrderBookConcept = requires(const BookType &book) {
  typename BookType::LevelKeyT;
  typename BookType::LevelValueT;
  typename BookType::BidIterator;
  typename BookType::AskIterator;
  typename BookType::BidBook;
  typename BookType::AskBook;

  {
    book.getBestBid()
  } -> std::same_as<std::optional<typename BookType::LevelKeyT>>;
  {
    book.getBestAsk()
  } -> std::same_as<std::optional<typename BookType::LevelKeyT>>;
  {
    book.bids()
  } -> std::same_as<std::pair<typename BookType::BidIterator,
                              typename BookType::BidIterator>>;
  {
    book.asks()
  } -> std::same_as<std::pair<typename BookType::AskIterator,
                              typename BookType::AskIterator>>;
};

} // namespace me