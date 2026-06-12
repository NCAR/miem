// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for the internal `ECCADReader` (src/internal/): CF time-units
// decoding, calendar enforcement, missing-time-variable handling, and
// rejection of files with no version attribute. Compiles against the
// internal header, which is not part of the installed surface.

#include "internal/eccad_reader.hpp"
#include "synthetic_nc.hpp"

#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

using miem::ECCADReader;
using miem::MiemException;
using miem_test::CreateTestNetCDF;
using miem_test::SyntheticNcOptions;
using miem_test::TempDir;

namespace {

// Precision-aware tolerance: tight under double, relaxed under float.
constexpr double kFluxTol =
    std::is_same_v<miem::Real, double> ? 1.0e-22 : 1.0e-15;

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
// A reference time carrying a UTC offset (e.g. "+01:00") is shifted back
// to UTC. 2012-01-01T00:00:00Z is 1325376000 s since the Unix epoch; a
// +01:00 reference names an instant one hour earlier in UTC.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceTimezoneOffsetShiftsToUtc)
{
  TempDir dir;
  const std::string path = dir.File("tzoffset.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00 +01:00";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 1325376000.0 - 3600.0,          1.0e-3);
  EXPECT_NEAR(times[1], 1325376000.0 - 3600.0 + 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// A fractional second in the reference ("...00.5") is retained in the
// decoded times rather than truncated.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceFractionalSecondsRetained)
{
  TempDir dir;
  const std::string path = dir.File("frac.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00.5";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 1325376000.5,          1.0e-3);
  EXPECT_NEAR(times[1], 1325376000.5 + 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// Fractional seconds and a UTC offset together are both applied.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceFractionalSecondsAndOffset)
{
  TempDir dir;
  const std::string path = dir.File("fractz.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00.5 +01:00";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 1325376000.5 - 3600.0,          1.0e-3);
  EXPECT_NEAR(times[1], 1325376000.5 - 3600.0 + 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// UTC offset, compact form (+/-[hh][mm], e.g. "+0130"). 1h30m ahead of UTC
// shifts the reference back by 5400 s -- not the 130 h a greedy parse gives.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceUtcOffsetCompactForm)
{
  TempDir dir;
  const std::string path = dir.File("tzcompact.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00 +0130";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 1325376000.0 - 5400.0,          1.0e-3);
  EXPECT_NEAR(times[1], 1325376000.0 - 5400.0 + 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// UTC offset, hours-only form (+/-[hh], e.g. "+01"). 1h ahead of UTC.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceUtcOffsetHoursOnly)
{
  TempDir dir;
  const std::string path = dir.File("tzhours.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00 +01";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  const auto times = r.GetTimeValues();
  ASSERT_EQ(times.size(), 2u);
  EXPECT_NEAR(times[0], 1325376000.0 - 3600.0,          1.0e-3);
  EXPECT_NEAR(times[1], 1325376000.0 - 3600.0 + 3600.0, 1.0e-3);
}

// ---------------------------------------------------------------------
// A 3-digit offset (matches no accepted form) is rejected, not parsed.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, ReferenceThreeDigitOffsetRejected)
{
  TempDir dir;
  const std::string path = dir.File("tz3.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00 +013";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), MiemException);
}

// ---------------------------------------------------------------------
// A units string we cannot fully account for (here a garbled offset) is
// rejected outright rather than silently truncated to the part we parsed.
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, MalformedReferenceOffsetRejected)
{
  TempDir dir;
  const std::string path = dir.File("badoffset.nc");
  SyntheticNcOptions opts;
  opts.time_units = "seconds since 2012-01-01 00:00:00 +bananas";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), MiemException);
}

// ---------------------------------------------------------------------
// calendar = "noleap" rejected with UnsupportedCalendar (MiemException)
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
  EXPECT_THROW(r.GetTimeValues(), MiemException);
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
// Missing units on time variable -> MiemException IO (invalid time units)
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
  EXPECT_THROW(r.GetTimeValues(), MiemException);
}

// ---------------------------------------------------------------------
// A time dimension of length > 1 with no `time` variable is a hard error
// (synthesized via SyntheticNcOptions::omit_time_variable).
// ---------------------------------------------------------------------
TEST(ECCADReaderTimeTest, MissingTimeVariableForMultipleStepsIsError)
{
  TempDir dir;
  const std::string path = dir.File("notimevar.nc");
  SyntheticNcOptions opts;
  opts.omit_time_variable = true;
  WriteSimpleFile(path, 2, 3, { 0.0, 0.0 }, 1.0e-9, opts);

  ECCADReader r;
  r.Open(path);
  EXPECT_THROW(r.GetTimeValues(), MiemException);
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
// A file with neither `eccad_version` nor `ses_version` is refused on
// Open (a hard error, not silently accepted).
// ---------------------------------------------------------------------
TEST(ECCADReaderVersionTest, NeitherVersionAttributeRejected)
{
  TempDir dir;
  const std::string path = dir.File("noversion.nc");
  SyntheticNcOptions opts;
  opts.eccad_version = "";
  opts.ses_version   = "";
  WriteSimpleFile(path, 2, 3, { 0.0, 3600.0 }, 1.0e-9, opts);

  ECCADReader r;
  EXPECT_THROW(r.Open(path), MiemException);
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
