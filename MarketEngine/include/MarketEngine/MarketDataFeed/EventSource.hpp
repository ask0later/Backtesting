#pragma once
#include "MarketEngine/Common/Order.hpp"
#include "MarketEngine/OrderBook/BookEvent.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace me {

using EventType =
    std::variant<AddOrderEvent, CancelOrderEvent, ModifyOrderEvent,
                 ExecuteMarketEvent, SnapshotUpdateEvent>;

struct Event final {
  TimestampNsT timestamp{};
  EventType event;
};

template <typename Book> void apply(Book &&bk, EventType &event) {
  std::visit(
      [&](auto &&visitor) {
        std::forward<Book>(bk).accept(std::forward<decltype(visitor)>(visitor));
      },
      event);
}

template <typename ConcreteEventSource> class EventSource {
public:
  // O(1) without redudant computations (parsing and so on)
  std::optional<Event> nextEvent() {
    return (cursor_ < events_.size()) ? std::optional(events_[cursor_++])
                                      : std::nullopt;
  }

  size_t remaining() const { return events_.size() - cursor_; }

  void reset() { cursor_ = 0; }

protected:
  explicit EventSource(std::vector<Event> &&events)
      : events_(std::move(events)), cursor_(0) {}

private:
  std::vector<Event> events_;
  size_t cursor_ = 0;
};

class LobSnapshotEventSource final
    : public EventSource<LobSnapshotEventSource> {
public:
  explicit LobSnapshotEventSource(const std::filesystem::path &lobCsvPath);
};

class TradesEventSource final : public EventSource<TradesEventSource> {
public:
  explicit TradesEventSource(const std::filesystem::path &tradesCsvPath);
};

class MixedEventSource final : public EventSource<MixedEventSource> {
public:
  MixedEventSource(const std::filesystem::path &lobCsvPath,
                   const std::filesystem::path &tradesCsvPath);
};

} // namespace me