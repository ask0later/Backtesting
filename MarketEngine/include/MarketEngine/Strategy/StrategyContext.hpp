#pragma once

#include "MarketEngine/Common/OrderRequest.hpp"

#include <utility>
#include <vector>

namespace me {

template <OrderBookConcept BookType> class StrategyContext final {
public:
  explicit StrategyContext(const BookType &book) : book_(book) {}

  const BookType &book() const { return book_; }

  void submitOrder(OrderRequest &&request) {
    requests_.push_back(std::move(request));
  }

  const std::vector<OrderRequest> &requests() const { return requests_; }

  std::vector<OrderRequest> drainRequests() {
    auto out = std::move(requests_);
    requests_.clear();
    return out;
  }

private:
  const BookType &book_;
  std::vector<OrderRequest> requests_;
};

} // namespace me