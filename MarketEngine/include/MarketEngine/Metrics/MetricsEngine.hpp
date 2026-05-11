#pragma once

#include "MarketEngine/Execution/ExecutionEvent.hpp"
#include "MarketEngine/Metrics/MetricsEngineContext.hpp"

namespace me {

namespace detail {

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace detail

class MetricsEngine final {
public:
  explicit MetricsEngine(MetricsEngineContext &ctx) : ctx_(ctx) {}

  void observe(const ExecutionEvent &ev) {
    std::visit(detail::Overloaded{
                   [this](const Fill &f) {
                     ctx_.portfolio.applyFill(f);
                     appendSnapshotIfNeeded();
                   },
                   [](const ExecutionReport &) {},
               },
               ev);
  }

  void setMarkToMarket(PriceT price) { ctx_.setMarkPrice(price); }

  void clearMarkToMarket() { ctx_.clearMarkPrice(); }

  const MetricsEngineContext &context() const { return ctx_; }
  MetricsEngineContext &context() { return ctx_; }

private:
  void appendSnapshotIfNeeded() {
    if (ctx_.recordSnapshots) {
      ctx_.snapshotHistory.push_back(ctx_.portfolio.snapshot());
    }
  }

  MetricsEngineContext &ctx_;
};

} // namespace me