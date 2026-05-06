#pragma once

#include "MarketEngine/Common/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <rapidcsv.h>
#include <string>
#include <string_view>

namespace me {

class CsvTable final {
public:
  static CsvTable readFromCsv(const std::filesystem::path &path);

  int64_t numRows() const;
  int numColumns() const;

  int fieldIndex(std::string_view name) const;
  std::string columnName(int columnIndex) const;

  double doubleAt(int columnIndex, int64_t rowIndex) const;
  TimestampNsT timestampAt(int columnIndex, int64_t rowIndex) const;
  std::string utf8At(int columnIndex, int64_t rowIndex) const;

private:
  explicit CsvTable(rapidcsv::Document doc);

  rapidcsv::Document doc_;
};

} // namespace me
