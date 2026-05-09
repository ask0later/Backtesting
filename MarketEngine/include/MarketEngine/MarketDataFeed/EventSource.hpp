#pragma once

#include "MarketEngine/MarketDataFeed/MarketEvent.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace me {

template <typename Book> void apply(Book &&bk, MarketEventType &event) {
  std::visit(
      [&](auto &&visitor) {
        std::forward<Book>(bk).accept(std::forward<decltype(visitor)>(visitor));
      },
      event);
}

template <typename ConcreteEventSource> class MarketEventSource {
public:
  // O(1) without redudant computations (parsing and so on)
  std::optional<MarketEvent> nextEvent() {
    return (cursor_ < events_.size()) ? std::optional(events_[cursor_++])
                                      : std::nullopt;
  }

  size_t remaining() const { return events_.size() - cursor_; }

  void reset() { cursor_ = 0; }

protected:
  explicit MarketEventSource(std::vector<MarketEvent> &&events)
      : events_(std::move(events)), cursor_(0) {}

private:
  std::vector<MarketEvent> events_;
  size_t cursor_ = 0;
};

class LobSnapshotEventSource final
    : public MarketEventSource<LobSnapshotEventSource> {
public:
  explicit LobSnapshotEventSource(const std::filesystem::path &lobCsvPath);
};

class TradesEventSource final : public MarketEventSource<TradesEventSource> {
public:
  explicit TradesEventSource(const std::filesystem::path &tradesCsvPath);
};

class MixedEventSource final : public MarketEventSource<MixedEventSource> {
public:
  MixedEventSource(const std::filesystem::path &lobCsvPath,
                   const std::filesystem::path &tradesCsvPath);
};

} // namespace me