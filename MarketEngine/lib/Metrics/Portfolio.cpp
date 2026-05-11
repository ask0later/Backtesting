#include "MarketEngine/Metrics/Portfolio.hpp"

#include <algorithm>
#include <cmath>

namespace me {

void Portfolio::applyFill(const Fill &fill) {
  const PriceT p = fill.price;
  const QtyT q = fill.qty;
  turnover_ += p * q;
  ++fillCount_;

  if (fill.side == Side::Buy) {
    buyVolume_ += q;
    onBuy(p, q);
  } else {
    sellVolume_ += q;
    onSell(p, q);
  }

  maxAbsInventory_ = std::max(maxAbsInventory_, std::abs(position_));
}

void Portfolio::setMarkPrice(PriceT mark) { markPrice_ = mark; }

void Portfolio::clearMarkPrice() { markPrice_.reset(); }

void Portfolio::reset() {
  cash_ = {};
  position_ = {};
  avgEntry_ = {};
  realizedPnl_ = {};
  turnover_ = {};
  maxAbsInventory_ = {};
  buyVolume_ = {};
  sellVolume_ = {};
  fillCount_ = {};
  markPrice_.reset();
}

PortfolioSnapshot Portfolio::snapshot() const {
  const PriceT unrealized = unrealizedPnl();
  return PortfolioSnapshot{.cash = cash_,
                           .inventory = position_,
                           .avgEntryPrice = avgEntry_,
                           .realizedPnl = realizedPnl_,
                           .unrealizedPnl = unrealized,
                           .totalPnl = realizedPnl_ + unrealized,
                           .turnover = turnover_,
                           .maxAbsInventory = maxAbsInventory_,
                           .buyVolume = buyVolume_,
                           .sellVolume = sellVolume_,
                           .fillCount = fillCount_,
                           .markPrice = markPrice_};
}

PriceT Portfolio::unrealizedPnl() const noexcept {
  if (!markPrice_.has_value() || position_ == 0.0) {
    return 0.0;
  }
  return position_ * (*markPrice_ - avgEntry_);
}

PriceT Portfolio::totalPnl() const noexcept {
  return realizedPnl_ + unrealizedPnl();
}

void Portfolio::onBuy(PriceT p, QtyT q) {
  cash_ -= p * q;

  if (position_ >= 0.0) {
    if (position_ == 0.0) {
      position_ = q;
      avgEntry_ = p;
      return;
    }
    const QtyT newPos = position_ + q;
    avgEntry_ = (position_ * avgEntry_ + q * p) / newPos;
    position_ = newPos;
    return;
  }

  const QtyT shortAbs = -position_;
  const QtyT cover = std::min(q, shortAbs);
  const QtyT openLong = q - cover;

  realizedPnl_ += cover * (avgEntry_ - p);

  if (openLong == 0.0) {
    position_ += q;
    if (position_ == 0.0) {
      avgEntry_ = 0.0;
    }
    return;
  }

  position_ = openLong;
  avgEntry_ = p;
}

void Portfolio::onSell(PriceT p, QtyT q) {
  cash_ += p * q;

  if (position_ <= 0.0) {
    if (position_ == 0.0) {
      position_ = -q;
      avgEntry_ = p;
      return;
    }
    const QtyT shortAbs = -position_;
    const QtyT newShortAbs = shortAbs + q;
    avgEntry_ = (shortAbs * avgEntry_ + q * p) / newShortAbs;
    position_ -= q;
    return;
  }

  const QtyT close = std::min(q, position_);
  const QtyT openShort = q - close;

  realizedPnl_ += close * (p - avgEntry_);

  if (openShort == 0.0) {
    position_ -= q;
    if (position_ == 0.0) {
      avgEntry_ = 0.0;
    }
    return;
  }

  position_ = -openShort;
  avgEntry_ = p;
}

} // namespace me
