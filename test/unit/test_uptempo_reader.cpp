// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for the internal `UptempoReader` (src/internal/): reading the
// real CAMS-on-MPAS black-carbon fixture (no version attribute, bc_anth_*
// variables, xtime strings, NaN-masked ocean cells) plus deterministic
// synthetic cases for xtime parsing, generic variable discovery, NaN
// handling, and the missing-time-variable rules. Compiles against the
// internal header, which is not part of the installed surface.

#include "internal/eccad_reader.hpp"
#include "internal/uptempo_reader.hpp"
#include "synthetic_nc.hpp"

#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using miem::ECCADReader;
using miem::MiemException;
using miem::Real;
using miem::UptempoReader;
using miem_test::CreateTestNetCDF;
using miem_test::CreateUptempoTestNetCDF;
using miem_test::TempDir;
using miem_test::UptempoNcOptions;

namespace {

// The real fixture committed at test/data/, located via the
// MIEM_TEST_DATA_DIR compile definition set by CMake.
std::string RealFixturePath()
{
  return std::string(MIEM_TEST_DATA_DIR) +
         "/CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc";
}

// 2012-01-01 00:00:00 UTC in seconds since the Unix epoch.
constexpr double kEpoch20120101 = 1325376000.0;

}  // namespace

// ---------------------------------------------------------------------
// Real fixture: opens despite having no version attribute, and reports the
// MPAS Time/nCells dimensions.
// ---------------------------------------------------------------------
TEST(UptempoReaderRealFixtureTest, OpensWithoutVersionAttribute)
{
  UptempoReader r;
  r.Open(RealFixturePath());
  EXPECT_TRUE(r.IsOpen());
  EXPECT_EQ(r.NumCells(), 4097);
  EXPECT_EQ(r.NumTimeSteps(), 12);
}

// ---------------------------------------------------------------------
// Real fixture: discovers every (Time, nCells) flux field by its own name,
// with no emi_ prefix and no hard-coded species list.
// ---------------------------------------------------------------------
TEST(UptempoReaderRealFixtureTest, DiscoversAllFluxVariables)
{
  UptempoReader r;
  r.Open(RealFixturePath());

  const auto species = r.QuerySpecies();
  // 11 anthropogenic BC sectors plus the precomputed bc_anth_sum.
  EXPECT_EQ(species.size(), 12u);
  EXPECT_NE(std::find(species.begin(), species.end(), "bc_anth_sum"),
            species.end());
  EXPECT_NE(std::find(species.begin(), species.end(), "bc_anth_ene"),
            species.end());
}

// ---------------------------------------------------------------------
// Real fixture: xtime MPAS strings decode to monotonically increasing
// seconds, starting at 2012-01-01 UTC.
// ---------------------------------------------------------------------
TEST(UptempoReaderRealFixtureTest, ParsesXtimeStamps)
{
  UptempoReader r;
  r.Open(RealFixturePath());

  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 12u);
  EXPECT_DOUBLE_EQ(times[0], kEpoch20120101);
  for (std::size_t i = 1; i < times.size(); ++i)
  {
    EXPECT_GT(times[i], times[i - 1]);
  }
}

// ---------------------------------------------------------------------
// Real fixture: reading bc_anth_sum yields finite, non-negative flux with
// the NaN-masked (ocean) cells zeroed -- the issue #16 smoke check.
// ---------------------------------------------------------------------
TEST(UptempoReaderRealFixtureTest, ReadFluxIsFiniteAndNonNegative)
{
  UptempoReader r;
  r.Open(RealFixturePath());

  std::vector<Real> flux;
  int               n_cells = 0;
  r.ReadFlux(/*time_index=*/0, { "bc_anth_sum" }, flux, n_cells);

  ASSERT_EQ(n_cells, 4097);
  ASSERT_EQ(flux.size(), static_cast<std::size_t>(n_cells));

  bool any_positive = false;
  for (const Real v : flux)
  {
    EXPECT_FALSE(std::isnan(static_cast<double>(v)));
    EXPECT_GE(static_cast<double>(v), 0.0);
    any_positive = any_positive || (static_cast<double>(v) > 0.0);
  }
  EXPECT_TRUE(any_positive);
}

// ---------------------------------------------------------------------
// Synthetic: xtime stamps decode to the expected epoch seconds.
// ---------------------------------------------------------------------
TEST(UptempoReaderSyntheticTest, XtimeDecodesToEpochSeconds)
{
  TempDir dir;
  const std::string path = dir.File("uptempo_xtime.nc");

  const std::vector<std::string> stamps = { "2012-01-01_00:00:00",
                                            "2012-01-01_06:00:00" };
  std::vector<double> data(2 * 3, 1.0e-9);
  CreateUptempoTestNetCDF(path, /*n_times=*/2, /*n_cells=*/3, stamps,
                          { "bc_anth_sum" }, { data });

  UptempoReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_DOUBLE_EQ(times[0], kEpoch20120101);
  EXPECT_DOUBLE_EQ(times[1], kEpoch20120101 + 6.0 * 3600.0);
}

// ---------------------------------------------------------------------
// Synthetic: the reader discovers arbitrary variable names, not just
// bc_anth_* -- it is not tied to one inventory's naming scheme.
// ---------------------------------------------------------------------
TEST(UptempoReaderSyntheticTest, DiscoversArbitraryVariableNames)
{
  TempDir dir;
  const std::string path = dir.File("uptempo_generic.nc");

  const std::vector<std::string> vars = { "co_anth", "nox_emis_total" };
  std::vector<double> co (3, 2.0e-9);
  std::vector<double> nox(3, 4.0e-9);
  CreateUptempoTestNetCDF(path, /*n_times=*/1, /*n_cells=*/3,
                          { "2012-01-01_00:00:00" }, vars, { co, nox });

  UptempoReader r;
  r.Open(path);
  const auto species = r.QuerySpecies();
  EXPECT_EQ(species.size(), 2u);
  EXPECT_NE(std::find(species.begin(), species.end(), "co_anth"),
            species.end());
  EXPECT_NE(std::find(species.begin(), species.end(), "nox_emis_total"),
            species.end());
}

// ---------------------------------------------------------------------
// Synthetic: NaN-masked cells read back as zero; real values are kept.
// ---------------------------------------------------------------------
TEST(UptempoReaderSyntheticTest, NanMaskedCellsZeroed)
{
  TempDir dir;
  const std::string path = dir.File("uptempo_nan.nc");

  // cell 0 real, cell 1 NaN (masked), cell 2 real.
  std::vector<double> data = { 3.0e-9, std::nan(""), 5.0e-9 };
  UptempoNcOptions opts;
  opts.nan_fill = true;
  CreateUptempoTestNetCDF(path, /*n_times=*/1, /*n_cells=*/3,
                          { "2012-01-01_00:00:00" }, { "bc_anth_sum" },
                          { data }, opts);

  UptempoReader r;
  r.Open(path);
  std::vector<Real> flux;
  int               n_cells = 0;
  r.ReadFlux(0, { "bc_anth_sum" }, flux, n_cells);

  ASSERT_EQ(flux.size(), 3u);
  EXPECT_FALSE(std::isnan(static_cast<double>(flux[1])));
  EXPECT_DOUBLE_EQ(static_cast<double>(flux[1]), 0.0);
  EXPECT_GT(static_cast<double>(flux[0]), 0.0);
  EXPECT_GT(static_cast<double>(flux[2]), 0.0);
}

// ---------------------------------------------------------------------
// Synthetic: a multi-step file with no xtime is malformed -- the reader
// refuses to fabricate time coordinates.
// ---------------------------------------------------------------------
TEST(UptempoReaderSyntheticTest, MissingXtimeMultiStepRejected)
{
  TempDir dir;
  const std::string path = dir.File("uptempo_noxtime.nc");

  std::vector<double> data(2 * 3, 1.0e-9);
  UptempoNcOptions opts;
  opts.omit_xtime = true;
  CreateUptempoTestNetCDF(path, /*n_times=*/2, /*n_cells=*/3, {},
                          { "bc_anth_sum" }, { data }, opts);

  UptempoReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), MiemException);
}

// ---------------------------------------------------------------------
// An ECCAD-layout file (n_cells dim, no nCells) is not a UPTEMPO file:
// Open rejects it rather than guessing.
// ---------------------------------------------------------------------
TEST(UptempoReaderSyntheticTest, RejectsEccadLayoutFile)
{
  TempDir dir;
  const std::string path = dir.File("eccad_layout.nc");

  std::vector<double> data(2 * 3, 1.0e-9);
  CreateTestNetCDF(path, /*n_times=*/2, /*n_cells=*/3, { 0.0, 3600.0 },
                   { "NOx" }, { data });

  UptempoReader r;
  EXPECT_THROW(r.Open(path), MiemException);
}
