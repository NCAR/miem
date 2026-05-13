// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for the internal `ECCADReader` (src/internal/) — CF time-
// units decoding, calendar enforcement, S2 missing-time-variable
// promotion, S4 empty-version-sentinel rejection.  This test compiles
// against the internal header; it is *not* exposed through any
// installed surface.

#include "internal/eccad_reader.hpp"
#include "synthetic_nc.hpp"

#include <miem/util/error.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using miem::ECCADReader;
using miem::IOError;
using miem_test::CreateTestNetCDF;
using miem_test::SyntheticNcOptions;
using miem_test::TempDir;

namespace {

// Precision-aware tolerance: tight under double, relaxed under float.
#ifdef MIEM_USE_DOUBLE
constexpr double kFluxTol = 1.0e-22;
#else
constexpr double kFluxTol = 1.0e-15;
#endif

// Helper: build a single-species file with one flux value per cell.
void WriteSimpleFile(const std::string&                       path,
                     int                                      n_times,
                     int                                      n_cells,
                     const std::vector<double>&               times,
                     double                                   flux_value,
                     const SyntheticNcOptions&                opts = {})
{
  std::vector<double> data(static_cast<std::size_t>(n_times) * n_cells,
                           flux_value);
  CreateTestNetCDF(path, n_times, n_cells, times, { "NOx" },
                   { data }, opts);
}

}  // namespace

// ---------------------------------------------------------------------
// units = "seconds since 1970-01-01" decodes (1 ms tolerance)
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, SecondsSinceEpoch)
{
  TempDir dir;
  const std::string path = dir.File("seconds.nc");
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 0.0,    1.0e-3);   // 1 ms tolerance
  EXPECT_NEAR(times[1], 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// units = "days since 2010-01-01" decodes (account for ref epoch)
// 2010-01-01T00:00Z = 1262304000 seconds since unix epoch.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, DaysSince2010)
{
  TempDir dir;
  const std::string path = dir.File("days.nc");
  SyntheticNcOptions opts;
  opts.time_units = "days since 2010-01-01";

  WriteSimpleFile(path, 2, 3, { 0.0, 1.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);

  // 2010-01-01 UTC -> 1262304000 seconds since Unix epoch.
  EXPECT_NEAR(times[0], 1262304000.0, 1.0e-3);
  // +1 day = +86400 s
  EXPECT_NEAR(times[1], 1262304000.0 + 86400.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// units = "hours since 1990-06-15 12:00:00" decodes
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, HoursSinceMid1990)
{
  TempDir dir;
  const std::string path = dir.File("hours.nc");
  SyntheticNcOptions opts;
  opts.time_units = "hours since 1990-06-15 12:00:00";

  WriteSimpleFile(path, 2, 3, { 0.0, 24.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);

  // 1990-06-15T12:00:00Z = 645408000 unix seconds.
  EXPECT_NEAR(times[0], 645451200.0,            1.0e-3);
  EXPECT_NEAR(times[1], 645451200.0 + 86400.0,  1.0e-3);
}

// ---------------------------------------------------------------------
// calendar = "noleap" rejected with UnsupportedCalendar (IOError)
// ---------------------------------------------------------------------
TEST(ECCADReaderCalendarTest, NoLeapRejected)
{
  TempDir dir;
  const std::string path = dir.File("noleap.nc");
  SyntheticNcOptions opts;
  opts.calendar = "noleap";
  WriteSimpleFile(path, 2, 3, { 0.0, 86400.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), IOError);
}

// ---------------------------------------------------------------------
// Missing calendar attribute -> accepted as proleptic Gregorian
// ---------------------------------------------------------------------
TEST(ECCADReaderCalendarTest, MissingCalendarAccepted)
{
  TempDir dir;
  const std::string path = dir.File("nocal.nc");
  SyntheticNcOptions opts;
  opts.calendar = "";  // suppress attribute
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_NO_THROW({
    auto times = r.GetTimeValues();
    EXPECT_EQ(times.size(), 2u);
  });
}

// ---------------------------------------------------------------------
// Missing units on time variable -> IOError (InvalidTimeUnits)
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, MissingUnitsAttributeRejected)
{
  TempDir dir;
  const std::string path = dir.File("nounits.nc");
  SyntheticNcOptions opts;
  opts.time_units = "";   // suppress
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), IOError);
}

// ---------------------------------------------------------------------
// S2 regression: time dim of length > 1 but no `time` variable -> hard
// error (climatology kill complement).  Synthesize via
// SyntheticNcOptions::omit_time_variable.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, S2_MissingTimeVariableForMultipleStepsIsError)
{
  TempDir dir;
  const std::string path = dir.File("notimevar.nc");
  SyntheticNcOptions opts;
  opts.omit_time_variable = true;
  WriteSimpleFile(path, 2, 3, { 0.0, 0.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), IOError);
}

// Single-snapshot file (n_times==1, no time var) is acceptable; reader
// returns [0.0] without throwing.
TEST(ECCADReaderTimeTest, MissingTimeVariableForSingleSnapshotAccepted)
{
  TempDir dir;
  const std::string path = dir.File("snapshot.nc");
  SyntheticNcOptions opts;
  opts.omit_time_variable = true;
  WriteSimpleFile(path, 1, 3, {}, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_NO_THROW({
    auto t = r.GetTimeValues();
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0], 0.0);
  });
}

// ---------------------------------------------------------------------
// S4 regression: file with neither eccad_version nor ses_version global
// attribute -> hard error on Open (refused, not silently accepted).
// ---------------------------------------------------------------------
TEST(ECCADReaderVersionTest, S4_NeitherVersionAttributeRejected)
{
  TempDir dir;
  const std::string path = dir.File("noversion.nc");
  SyntheticNcOptions opts;
  opts.eccad_version = "";
  opts.ses_version   = "";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  EXPECT_THROW(r.Open(path), IOError);
}

TEST(ECCADReaderVersionTest, LegacySesVersionAccepted)
{
  TempDir dir;
  const std::string path = dir.File("sesonly.nc");
  SyntheticNcOptions opts;
  opts.eccad_version = "";
  opts.ses_version   = "1.0";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  EXPECT_NO_THROW(r.Open(path));
  EXPECT_EQ(r.EccadVersion(), "1.0");  // populated from ses_version
}

// ---------------------------------------------------------------------
// Species discovery: `emi_<species>` variables are surfaced.
// ---------------------------------------------------------------------
TEST(ECCADReaderTest, DiscoverSpecies)
{
  TempDir dir;
  const std::string path = dir.File("multi.nc");

  std::vector<double> data(2 * 3, 1.0e-9);
  CreateTestNetCDF(path, 2, 3, { 0.0, 3600.0 },
                   { "NOx", "SO2" }, { data, data });

  ECCADReader r;
  r.Open(path);
  auto species = r.QuerySpecies();
  std::sort(species.begin(), species.end());
  ASSERT_EQ(species.size(), 2u);
  EXPECT_EQ(species[0], "NOx");
  EXPECT_EQ(species[1], "SO2");
}

// ReadFlux round-trips written values.
TEST(ECCADReaderTest, ReadFluxRoundTrip)
{
  TempDir dir;
  const std::string path = dir.File("rdflux.nc");

  // 2 times x 3 cells; NOx flux row-major [t, c]:
  //   t=0: 1e-9, 2e-9, 3e-9
  //   t=1: 4e-9, 5e-9, 6e-9
  std::vector<double> data = {
      1.0e-9, 2.0e-9, 3.0e-9,
      4.0e-9, 5.0e-9, 6.0e-9,
  };
  CreateTestNetCDF(path, 2, 3, { 0.0, 3600.0 },
                   { "NOx" }, { data });

  ECCADReader r;
  r.Open(path);

  std::vector<miem::Real> out;
  int n_cells_out = 0;
  r.ReadFlux(1, { "NOx" }, out, n_cells_out);
  ASSERT_EQ(n_cells_out, 3);
  ASSERT_EQ(out.size(), 3u);
  EXPECT_NEAR(static_cast<double>(out[0]), 4.0e-9, kFluxTol);
  EXPECT_NEAR(static_cast<double>(out[1]), 5.0e-9, kFluxTol);
  EXPECT_NEAR(static_cast<double>(out[2]), 6.0e-9, kFluxTol);
}
