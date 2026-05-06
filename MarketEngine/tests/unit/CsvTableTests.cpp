#include "MarketEngine/DataSource/Csv/CsvTable.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

namespace fs = std::filesystem;

static void checkColumnName(const me::CsvTable &t, int idx,
                            std::string_view expected) {
  EXPECT_EQ(t.columnName(idx), expected);
}

TEST(CsvTable, LobLoadsCorrectShape) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  EXPECT_EQ(t.numRows(), 25);
  EXPECT_EQ(t.numColumns(), 102);
}

TEST(CsvTable, LobColumnNamesAndIndices) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  EXPECT_EQ(t.columnName(0), "");
  EXPECT_EQ(t.columnName(1), "local_timestamp");
  EXPECT_EQ(t.fieldIndex("local_timestamp"), 1);
  EXPECT_EQ(t.fieldIndex("asks[0].price"), 2);
  EXPECT_EQ(t.fieldIndex("asks[0].amount"), 3);
  EXPECT_EQ(t.fieldIndex("bids[0].price"), 4);
  EXPECT_EQ(t.fieldIndex("bids[0].amount"), 5);
  EXPECT_EQ(t.fieldIndex("asks[1].price"), 6);
  EXPECT_EQ(t.fieldIndex("bids[24].amount"), 101);
  EXPECT_LT(t.fieldIndex("no_such_column"), 0);
}

TEST(CsvTable, LobFirstRowSnapShotValues) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  const int64_t row = 0;
  const int colTs = t.fieldIndex("local_timestamp");
  ASSERT_GE(colTs, 0);
  EXPECT_EQ(t.timestampAt(colTs, row), 1722470402038431LL);
  const int ask0px = t.fieldIndex("asks[0].price");
  const int ask0amt = t.fieldIndex("asks[0].amount");
  const int bid0px = t.fieldIndex("bids[0].price");
  const int bid0amt = t.fieldIndex("bids[0].amount");
  EXPECT_NEAR(t.doubleAt(ask0px, row), 0.0110436, 1e-12);
  EXPECT_NEAR(t.doubleAt(ask0amt, row), 121492.0, 1e-9);
  EXPECT_NEAR(t.doubleAt(bid0px, row), 0.0110435, 1e-12);
  EXPECT_NEAR(t.doubleAt(bid0amt, row), 103687.0, 1e-9);
}

TEST(CsvTable, LobSeveralRowsAndLevels) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  const int64_t row2 = 2;
  const int colTs = t.fieldIndex("local_timestamp");
  EXPECT_EQ(t.timestampAt(colTs, row2), 1722470403485121LL);
  const int ask1px = t.fieldIndex("asks[1].price");
  const int ask1amt = t.fieldIndex("asks[1].amount");
  const int bid1px = t.fieldIndex("bids[1].price");
  const int bid1amt = t.fieldIndex("bids[1].amount");
  EXPECT_NEAR(t.doubleAt(ask1px, row2), 0.0110412, 1e-12);
  EXPECT_NEAR(t.doubleAt(ask1amt, row2), 59430.0, 1e-9);
  EXPECT_NEAR(t.doubleAt(bid1px, row2), 0.0110404, 1e-12);
  EXPECT_NEAR(t.doubleAt(bid1amt, row2), 337839.0, 1e-9);
  const int64_t lastRow = t.numRows() - 1;
  const int ask24px = t.fieldIndex("asks[24].price");
  const int ask24amt = t.fieldIndex("asks[24].amount");
  EXPECT_NEAR(t.doubleAt(ask24px, lastRow), 0.0110296, 1e-12);
  EXPECT_NEAR(t.doubleAt(ask24amt, lastRow), 794338.0, 1e-9);
}

TEST(CsvTable, LobOutOfBoundsThrows) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  EXPECT_THROW(t.doubleAt(200, 0), std::out_of_range);
  EXPECT_THROW(t.utf8At(200, 0), std::out_of_range);
  EXPECT_THROW(t.timestampAt(200, 0), std::out_of_range);
  EXPECT_THROW(t.columnName(200), std::out_of_range);
  EXPECT_THROW(t.doubleAt(0, t.numRows()), std::out_of_range);
  EXPECT_THROW(t.utf8At(0, -1), std::out_of_range);
}

TEST(CsvTable, TradesLoadsCorrectShape) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_trades.csv");
  EXPECT_EQ(t.numRows(), 29);
  EXPECT_EQ(t.numColumns(), 5);
}

TEST(CsvTable, TradesColumnNamesAndIndices) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_trades.csv");
  EXPECT_EQ(t.columnName(0), "");
  EXPECT_EQ(t.columnName(1), "local_timestamp");
  EXPECT_EQ(t.columnName(2), "side");
  EXPECT_EQ(t.columnName(3), "price");
  EXPECT_EQ(t.columnName(4), "amount");
  EXPECT_EQ(t.fieldIndex("local_timestamp"), 1);
  EXPECT_EQ(t.fieldIndex("side"), 2);
  EXPECT_EQ(t.fieldIndex("price"), 3);
  EXPECT_EQ(t.fieldIndex("amount"), 4);
  EXPECT_LT(t.fieldIndex("xyz"), 0);
}

TEST(CsvTable, TradesFirstRowValues) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_trades.csv");
  const int64_t row0 = 0;
  const int colTs = t.fieldIndex("local_timestamp");
  const int colSide = t.fieldIndex("side");
  const int colPrice = t.fieldIndex("price");
  const int colAmt = t.fieldIndex("amount");
  EXPECT_EQ(t.timestampAt(colTs, row0), 1722470400014926LL);
  EXPECT_EQ(t.utf8At(colSide, row0), "sell");
  EXPECT_NEAR(t.doubleAt(colPrice, row0), 0.0110435, 1e-12);
  EXPECT_NEAR(t.doubleAt(colAmt, row0), 734.0, 1e-9);
}

TEST(CsvTable, TradesMultipleRows) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_trades.csv");
  const int64_t row4 = 4;
  const int colSide = t.fieldIndex("side");
  const int colPrice = t.fieldIndex("price");
  const int colAmt = t.fieldIndex("amount");
  EXPECT_EQ(t.utf8At(colSide, row4), "buy");
  EXPECT_NEAR(t.doubleAt(colPrice, row4), 0.0110436, 1e-12);
  EXPECT_NEAR(t.doubleAt(colAmt, row4), 5378.0, 1e-9);
  const int64_t row21 = 21;
  EXPECT_EQ(t.utf8At(colSide, row21), "sell");
  EXPECT_NEAR(t.doubleAt(colPrice, row21), 0.0110421, 1e-12);
  EXPECT_NEAR(t.doubleAt(colAmt, row21), 199830.0, 1e-9);
  const int64_t lastRow = t.numRows() - 1;
  EXPECT_EQ(t.timestampAt(t.fieldIndex("local_timestamp"), lastRow),
            1722470403094140LL);
  EXPECT_EQ(t.utf8At(colSide, lastRow), "sell");
  EXPECT_NEAR(t.doubleAt(colPrice, lastRow), 0.0110412, 1e-12);
  EXPECT_NEAR(t.doubleAt(colAmt, lastRow), 5157.0, 1e-9);
}

TEST(CsvTable, TradesOutOfBoundsThrows) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_trades.csv");
  EXPECT_THROW(t.doubleAt(10, 0), std::out_of_range);
  EXPECT_THROW(t.utf8At(10, 0), std::out_of_range);
  EXPECT_THROW(t.timestampAt(10, 0), std::out_of_range);
  EXPECT_THROW(t.columnName(10), std::out_of_range);
  EXPECT_THROW(t.doubleAt(0, 1000), std::out_of_range);
}

TEST(CsvTable, ConsistentColumnNameAndFieldIndex) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  for (int col = 0; col < t.numColumns(); ++col) {
    std::string name = t.columnName(col);
    int idx = t.fieldIndex(name);
    EXPECT_EQ(idx, col) << "Mismatch for column '" << name << "' at index "
                        << col;
  }
}

TEST(CsvTable, TimestampNeverZero) {
  const me::CsvTable t = me::CsvTable::readFromCsv("data/small_lob.csv");
  const int colTs = t.fieldIndex("local_timestamp");
  for (int64_t row = 0; row < t.numRows(); ++row) {
    EXPECT_GT(t.timestampAt(colTs, row), 0) << "row " << row;
  }
}

TEST(CsvTable, PriceAndAmountPositive) {
  const me::CsvTable lob = me::CsvTable::readFromCsv("data/small_lob.csv");
  const int ask0amt = lob.fieldIndex("asks[0].amount");
  const int bid0amt = lob.fieldIndex("bids[0].amount");
  for (int64_t row = 0; row < lob.numRows(); ++row) {
    EXPECT_GE(lob.doubleAt(ask0amt, row), 0.0);
    EXPECT_GE(lob.doubleAt(bid0amt, row), 0.0);
  }
  const me::CsvTable trades =
      me::CsvTable::readFromCsv("data/small_trades.csv");
  const int colAmt = trades.fieldIndex("amount");
  const int colPrice = trades.fieldIndex("price");
  for (int64_t row = 0; row < trades.numRows(); ++row) {
    EXPECT_GT(trades.doubleAt(colAmt, row), 0.0);
    EXPECT_GT(trades.doubleAt(colPrice, row), 0.0);
  }
}