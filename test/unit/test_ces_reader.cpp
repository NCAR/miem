#include <gtest/gtest.h>

#include "miem/ces_reader.hpp"

using namespace miem;

// CESReader tests require NetCDF test data files.
// These tests verify the interface; full I/O tests need generated test data.

TEST(CESReader, DefaultState) {
  CESReader reader;
  EXPECT_FALSE(reader.IsOpen());
  EXPECT_EQ(reader.NumCells(), 0);
  EXPECT_EQ(reader.NumTimeSteps(), 0);
}

TEST(CESReader, OpenNonexistentFileThrows) {
  CESReader reader;
  EXPECT_THROW(reader.Open("nonexistent_file.nc"), IOError);
}

TEST(CESReader, ReadFluxWithoutOpenThrows) {
  CESReader reader;
  std::vector<Real> flux;
  int n_cells;
  EXPECT_THROW(reader.ReadFlux(0, {"NO"}, flux, n_cells), IOError);
}
