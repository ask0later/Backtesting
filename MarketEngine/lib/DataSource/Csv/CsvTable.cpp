#include "MarketEngine/DataSource/Csv/CsvTable.hpp"

#include <limits>
#include <stdexcept>

namespace {

rapidcsv::Document openDoc(const std::filesystem::path &path) {
  rapidcsv::ConverterParams conv(
      false, std::numeric_limits<long double>::quiet_NaN(), 0, false);
  return rapidcsv::Document(path.string(), rapidcsv::LabelParams(),
                            rapidcsv::SeparatorParams(), conv);
}

} // namespace

namespace me {

CsvTable CsvTable::readFromCsv(const std::filesystem::path &path) {
  return CsvTable(openDoc(path));
}

CsvTable::CsvTable(rapidcsv::Document doc) : doc_(std::move(doc)) {}

int64_t CsvTable::numRows() const {
  return static_cast<int64_t>(doc_.GetRowCount());
}

int CsvTable::numColumns() const {
  return static_cast<int>(doc_.GetColumnCount());
}

int CsvTable::fieldIndex(std::string_view name) const {
  return doc_.GetColumnIdx(std::string(name));
}

std::string CsvTable::columnName(int columnIndex) const {
  if (columnIndex < 0 || columnIndex >= numColumns()) {
    throw std::out_of_range("columnName: invalid column index " +
                            std::to_string(columnIndex));
  }
  return doc_.GetColumnName(static_cast<size_t>(columnIndex));
}

double CsvTable::doubleAt(int col, int64_t row) const {
  if (col < 0 || col >= numColumns()) {
    throw std::out_of_range("doubleAt: invalid column index " +
                            std::to_string(col));
  }
  if (row < 0 || row >= numRows()) {
    throw std::out_of_range("doubleAt: invalid row index " +
                            std::to_string(row));
  }
  return doc_.GetCell<double>(static_cast<size_t>(col),
                              static_cast<size_t>(row));
}

TimestampNsT CsvTable::timestampAt(int col, int64_t row) const {
  if (col < 0 || col >= numColumns()) {
    throw std::out_of_range("timestampAt: invalid column index " +
                            std::to_string(col));
  }
  if (row < 0 || row >= numRows()) {
    throw std::out_of_range("timestampAt: invalid row index " +
                            std::to_string(row));
  }
  return static_cast<TimestampNsT>(doc_.GetCell<long long>(
      static_cast<size_t>(col), static_cast<size_t>(row)));
}

std::string CsvTable::utf8At(int col, int64_t row) const {
  if (col < 0 || col >= numColumns()) {
    throw std::out_of_range("utf8At: invalid column index " +
                            std::to_string(col));
  }
  if (row < 0 || row >= numRows()) {
    throw std::out_of_range("utf8At: invalid row index " + std::to_string(row));
  }
  return doc_.GetCell<std::string>(static_cast<size_t>(col),
                                   static_cast<size_t>(row));
}

} // namespace me