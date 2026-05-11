#pragma once

#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/Metrics/Portfolio.hpp"

namespace me {

struct MetricsEngineContext final {
  Portfolio portfolio{};
  std::optional<PriceT> lastMarkPrice{};
  bool recordSnapshots{false};
  std::vector<PortfolioSnapshot> snapshotHistory{};

  void setMarkPrice(PriceT m) {
    lastMarkPrice = m;
    portfolio.setMarkPrice(m);
  }

  void clearMarkPrice() {
    lastMarkPrice.reset();
    portfolio.clearMarkPrice();
  }
};

} // namespace me
