#pragma once

#include "MarketEngine/Common/Types.hpp"
#include "MarketEngine/Execution/Fill.hpp"

#include <cstddef>
#include <optional>

namespace me {

struct PortfolioSnapshot final {
  PriceT cash{};
  QtyT inventory{};
  PriceT avgEntryPrice{};
  PriceT realizedPnl{};
  PriceT unrealizedPnl{};
  PriceT totalPnl{};
  PriceT turnover{};
  QtyT maxAbsInventory{};
  QtyT buyVolume{};
  QtyT sellVolume{};
  std::size_t fillCount{};
  std::optional<PriceT> markPrice;
};

class Portfolio final {
public:
  void applyFill(const Fill &fill);

  void setMarkPrice(PriceT mark);
  void clearMarkPrice();
  void reset();

  PortfolioSnapshot snapshot() const;

  PriceT cash() const { return cash_; }
  QtyT inventory() const { return position_; }
  PriceT avgEntryPrice() const { return avgEntry_; }
  PriceT realizedPnl() const { return realizedPnl_; }
  PriceT unrealizedPnl() const noexcept;
  PriceT totalPnl() const noexcept;
  PriceT turnover() const { return turnover_; }
  QtyT maxAbsInventory() const { return maxAbsInventory_; }
  QtyT buyVolume() const { return buyVolume_; }
  QtyT sellVolume() const { return sellVolume_; }
  std::size_t fillCount() const { return fillCount_; }
  const std::optional<PriceT> &markPrice() const { return markPrice_; }

private:
  void onBuy(PriceT price, QtyT qty);
  void onSell(PriceT price, QtyT qty);

  PriceT cash_{};
  QtyT position_{}; // >0 long, <0 short
  PriceT avgEntry_{};
  PriceT realizedPnl_{};
  PriceT turnover_{};
  QtyT maxAbsInventory_{};
  QtyT buyVolume_{};
  QtyT sellVolume_{};
  std::size_t fillCount_{};
  std::optional<PriceT> markPrice_{};
};

} // namespace me