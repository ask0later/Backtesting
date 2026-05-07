#include "MarketEngine/MarketDataFeed/EventSource.hpp"

#include "MarketEngine/DataSource/Csv/CsvTable.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace me {

namespace {

namespace fs = std::filesystem;

CsvTable loadCsv(const fs::path &path) { return CsvTable::readFromCsv(path); }

std::string toLower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool parseSideTrade(std::string_view s) {
  const std::string l = toLower(std::string(s));
  if (l == "buy" || l == "b")
    return true;
  if (l == "sell" || l == "s")
    return false;
  throw std::runtime_error("unknown trade side: " + std::string(s));
}

struct LobLevelCols {
  int askPx{-1};
  int askAmt{-1};
  int bidPx{-1};
  int bidAmt{-1};
};

bool startsWith(std::string_view s, std::string_view pref) {
  return s.size() >= pref.size() && s.compare(0, pref.size(), pref) == 0;
}

void registerLobColumn(std::unordered_map<int, LobLevelCols> &byLevel,
                       std::string_view h, int col) {
  if (h.empty())
    return;
  const bool isAsk = startsWith(h, "asks[");
  const bool isBid = startsWith(h, "bids[");
  if (!isAsk && !isBid)
    return;

  const auto lb = h.find('[');
  const auto rb = h.find(']');
  if (lb == std::string_view::npos || rb == std::string_view::npos || rb <= lb)
    return;
  int level = 0;
  {
    const auto sub = h.substr(lb + 1, rb - lb - 1);
    auto [ptr, ec] =
        std::from_chars(sub.data(), sub.data() + sub.size(), level);
    if (ec != std::errc{})
      level = std::stoi(std::string(sub));
  }
  const auto rest = h.substr(rb + 1);
  const bool isPrice = startsWith(rest, ".price");
  const bool isAmount = startsWith(rest, ".amount");
  if (!isPrice && !isAmount)
    return;

  LobLevelCols &L = byLevel[level];
  if (isAsk) {
    if (isPrice)
      L.askPx = col;
    else
      L.askAmt = col;
  } else {
    if (isPrice)
      L.bidPx = col;
    else
      L.bidAmt = col;
  }
}

SparseOrderBook::BidBook
buildBidsFromLevelMap(const std::unordered_map<int, LobLevelCols> &byLevel,
                      const CsvTable &table, int64_t row) {
  SparseOrderBook::BidBook bids;
  const int ncols = table.numColumns();
  for (const auto &[level, c] : byLevel) {
    if (c.bidPx < 0 || c.bidAmt < 0)
      continue;
    if (c.bidPx >= ncols || c.bidAmt >= ncols)
      continue;
    const PriceT px = table.doubleAt(c.bidPx, row);
    const QtyT amt = table.doubleAt(c.bidAmt, row);
    if (amt > 0.0)
      bids[px] = amt;
  }
  return bids;
}

SparseOrderBook::AskBook
buildAsksFromLevelMap(const std::unordered_map<int, LobLevelCols> &byLevel,
                      const CsvTable &table, int64_t row) {
  SparseOrderBook::AskBook asks;
  const int ncols = table.numColumns();
  for (const auto &[level, c] : byLevel) {
    (void)level;
    if (c.askPx < 0 || c.askAmt < 0)
      continue;
    if (c.askPx >= ncols || c.askAmt >= ncols)
      continue;
    const PriceT px = table.doubleAt(c.askPx, row);
    const QtyT amt = table.doubleAt(c.askAmt, row);
    if (amt > 0.0)
      asks[px] = amt;
  }
  return asks;
}

std::vector<Event> buildLobEvents(const CsvTable &table) {
  std::unordered_map<int, LobLevelCols> byLevel;
  byLevel.reserve(32);
  for (int c = 0; c < table.numColumns(); ++c)
    registerLobColumn(byLevel, table.columnName(c), c);

  const int colTs = table.fieldIndex("local_timestamp");
  if (colTs < 0)
    throw std::runtime_error("LOB CSV: column 'local_timestamp' not found");

  const int64_t nrows = table.numRows();
  std::vector<Event> out;
  out.reserve(static_cast<std::size_t>(std::max<int64_t>(nrows, 0)));
  for (int64_t r = 0; r < nrows; ++r) {
    Event ev{};
    ev.timestamp = table.timestampAt(colTs, r);
    SnapshotUpdateEvent sue;
    sue.newBids = buildBidsFromLevelMap(byLevel, table, r);
    sue.newAsks = buildAsksFromLevelMap(byLevel, table, r);
    ev.event = std::move(sue);
    out.push_back(std::move(ev));
  }
  return out;
}

std::vector<Event> buildTradeEvents(const CsvTable &table) {
  const int colTs = table.fieldIndex("local_timestamp");
  const int colSide = table.fieldIndex("side");
  const int colQty = table.fieldIndex("amount");
  if (colTs < 0 || colSide < 0 || colQty < 0)
    throw std::runtime_error("trades CSV: missing required columns");

  const int64_t nrows = table.numRows();
  std::vector<Event> out;
  out.reserve(std::max<int64_t>(nrows, 0));
  for (int64_t r = 0; r < nrows; ++r) {
    Event ev{};
    ev.timestamp = table.timestampAt(colTs, r);
    const bool buyer = parseSideTrade(table.utf8At(colSide, r));

    ExecuteMarketEvent ex{};
    ex.qty = table.doubleAt(colQty, r);
    ex.side = buyer ? Side::Buy : Side::Sell;
    ev.event = std::move(ex);
    out.push_back(std::move(ev));
  }
  return out;
}

std::vector<Event> mergeLobThenTradesByTimestamp(const CsvTable &lob,
                                                 const CsvTable &trades) {
  std::vector<Event> a = buildLobEvents(lob);
  std::vector<Event> b = buildTradeEvents(trades);
  std::vector<Event> out;
  out.reserve(a.size() + b.size());
  auto ai = a.begin();
  auto bi = b.begin();
  const auto ae = a.end();
  const auto be = b.end();
  while (ai != ae || bi != be) {
    if (bi == be || (ai != ae && ai->timestamp <= bi->timestamp))
      out.push_back(std::move(*ai++));
    else
      out.push_back(std::move(*bi++));
  }
  return out;
}

} // namespace

LobSnapshotEventSource::LobSnapshotEventSource(
    const std::filesystem::path &lobCsvPath)
    : EventSource(buildLobEvents(loadCsv(lobCsvPath))) {}

TradesEventSource::TradesEventSource(const std::filesystem::path &tradesCsvPath)
    : EventSource(buildTradeEvents(loadCsv(tradesCsvPath))) {}

MixedEventSource::MixedEventSource(const std::filesystem::path &lobCsvPath,
                                   const std::filesystem::path &tradesCsvPath)
    : EventSource(mergeLobThenTradesByTimestamp(loadCsv(lobCsvPath),
                                                loadCsv(tradesCsvPath))) {}

} // namespace me
