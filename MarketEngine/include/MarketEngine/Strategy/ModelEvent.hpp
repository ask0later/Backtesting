#pragma once

#include "MarketEngine/OrderBook/BookRequest.hpp"

#include <concepts>
#include <cstddef>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

namespace me {

// Events produced by the model/strategy side before exchange simulation.

using ModelEvent = std::variant<AddOrderRequest, CancelOrderRequest,
                                ModifyOrderRequest, MarketOrderRequest>;

template <typename RequestType>
concept ModelEventRequest =
    std::same_as<std::remove_cvref_t<RequestType>, AddOrderRequest> ||
    std::same_as<std::remove_cvref_t<RequestType>, CancelOrderRequest> ||
    std::same_as<std::remove_cvref_t<RequestType>, ModifyOrderRequest> ||
    std::same_as<std::remove_cvref_t<RequestType>, MarketOrderRequest>;

class ModelEventBuffer final {
public:
  bool empty() const { return events_.empty(); }
  size_t size() const { return events_.size(); }

  ModelEvent &front() { return events_.front(); }
  const ModelEvent &front() const { return events_.front(); }

  const std::vector<ModelEvent> &events() const { return events_; }
  std::span<const ModelEvent> view() const {
    return {events_.data(), events_.size()};
  }

  void reserve(size_t capacity) { events_.reserve(capacity); }
  void clear() { events_.clear(); }

  void push(ModelEvent event) { events_.push_back(std::move(event)); }

  template <ModelEventRequest RequestType> void push(RequestType &&request) {
    events_.emplace_back(std::forward<RequestType>(request));
  }

  std::vector<ModelEvent> drain() {
    std::vector<ModelEvent> out;
    out.swap(events_);
    return out;
  }

  std::vector<ModelEvent> snapshot() const { return events_; }

private:
  std::vector<ModelEvent> events_;
};

} // namespace me
