// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for `OfflineEmissionSource`.  Covers temporal blend,
// nearest-mode pick, climatology-kill regression (D5 / O2, O3),
// cell-count mismatch, and file-pattern token substitution.

#include "synthetic_nc.hpp"

#include <miem/source_types.hpp>
#include <miem/source_offline.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using miem::MiemException;
using miem::Real;
using miem::Source;
using miem::OfflineEmissionSource;
using miem::TemporalInterpolation;
using miem_test::CreateTestNetCDF;
using miem_test::SyntheticNcOptions;
using miem_test::TempDir;

// Assert that `stmt` throws a miem::MiemException carrying (cat, code).
#define EXPECT_MIEM_THROW(stmt, cat, code)                          \
  do {                                                              \
    try {                                                           \
      stmt;                                                         \
      ADD_FAILURE() << "expected miem::MiemException, none thrown"; \
    } catch (const ::miem::MiemException& e) {                      \
      EXPECT_STREQ(e.Category(), (cat));                            \
      EXPECT_EQ(e.Code(), (code));                                  \
    }                                                               \
  } while (0)

namespace {

// Precision-aware tolerance.
#ifdef MIEM_USE_DOUBLE
constexpr double kFluxTol = 1.0e-22;
#else
constexpr double kFluxTol = 1.0e-15;
#endif

// Two-time-step synthetic file at t=0 and t=3600, n_cells=3, single NOx
// species; flux row-major [t, cell].
std::string MakeTwoStepFile(const TempDir&            dir,
                            const std::vector<double>& nox_flux,
                            const std::string&         name = "test.nc")
{
  const std::string path = dir.File(name);
  CreateTestNetCDF(path, /*n_times=*/2, /*n_cells=*/3,
                   /*time_values=*/{ 0.0, 3600.0 },
                   /*species=*/{ "NOx" },
                   /*flux_data=*/{ nox_flux });
  return path;
}

// Identity species map: NOx -> NOx, scaling 1.0.  Keeps the test focused
// on the temporal / out-of-range behaviour.
Source MakeIdentitySource(const std::string&   path,
                                TemporalInterpolation interp =
                                    TemporalInterpolation::Linear)
{
  Source cfg;
  cfg.name_                   = "test_source";
  cfg.file_pattern_           = path;
  cfg.convention_             = "eccad";
  cfg.temporal_interpolation_ = interp;
  cfg.species_map_.AddMapping("NOx", "NOx", 1.0);
  return cfg;
}

}  // namespace

// ---------------------------------------------------------------------
// Linear: at t=1800s (midpoint) -> blend of t=0 (1e-9) and t=3600 (3e-9).
// Expected value: 2e-9 per cell.
// ---------------------------------------------------------------------
TEST(OfflineEmissionSourceTest, LinearMidpointBlend)
{
  TempDir dir;
  std::vector<double> flux = {
      1.0e-9, 1.0e-9, 1.0e-9,    // t=0
      3.0e-9, 3.0e-9, 3.0e-9     // t=3600
  };
  const std::string path = MakeTwoStepFile(dir, flux);

  Source cfg = MakeIdentitySource(path,
                                        TemporalInterpolation::Linear);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  ASSERT_NO_THROW(src.Update(/*time_current=*/1800.0, /*n_cells=*/3,
                             out, names));

  ASSERT_EQ(out.size(), 3u);
  for (auto v : out)
  {
    EXPECT_NEAR(static_cast<double>(v), 2.0e-9, kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Nearest mode: at t=2400 (closer to t=3600) -> picks right value.
// ---------------------------------------------------------------------
TEST(OfflineEmissionSourceTest, NearestPicksCloserEnd)
{
  TempDir dir;
  std::vector<double> flux = {
      1.0e-9, 1.0e-9, 1.0e-9,
      3.0e-9, 3.0e-9, 3.0e-9
  };
  const std::string path = MakeTwoStepFile(dir, flux);

  Source cfg = MakeIdentitySource(path,
                                        TemporalInterpolation::Nearest);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  ASSERT_NO_THROW(src.Update(2400.0, 3, out, names));

  for (auto v : out)
  {
    EXPECT_NEAR(static_cast<double>(v), 3.0e-9, kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Climatology kill (D5): time_current > times[n-1] -> TimeOutOfRange
// ---------------------------------------------------------------------
TEST(OfflineEmissionSourceTest, TimeAfterRangeReturnsTimeOutOfRange)
{
  TempDir dir;
  std::vector<double> flux(6, 1.0e-9);
  const std::string path = MakeTwoStepFile(dir, flux);

  Source cfg = MakeIdentitySource(path);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  EXPECT_MIEM_THROW(src.Update(/*time_current=*/9999999.0, 3, out, names),
                    MIEM_ERROR_CATEGORY_IO,
                    MIEM_IO_ERROR_CODE_TIME_OUT_OF_RANGE);
}

TEST(OfflineEmissionSourceTest, TimeBeforeRangeReturnsTimeOutOfRange)
{
  TempDir dir;
  std::vector<double> flux(6, 1.0e-9);
  const std::string path = MakeTwoStepFile(dir, flux);

  Source cfg = MakeIdentitySource(path);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  EXPECT_MIEM_THROW(src.Update(/*time_current=*/-1.0, 3, out, names),
                    MIEM_ERROR_CATEGORY_IO,
                    MIEM_IO_ERROR_CODE_TIME_OUT_OF_RANGE);
}

// ---------------------------------------------------------------------
// Cell-count mismatch: file has 3 cells, source asked for 5 -> error.
// ---------------------------------------------------------------------
TEST(OfflineEmissionSourceTest, CellCountMismatchBetweenFileAndCaller)
{
  TempDir dir;
  std::vector<double> flux(6, 1.0e-9);
  const std::string path = MakeTwoStepFile(dir, flux);

  Source cfg = MakeIdentitySource(path);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  EXPECT_MIEM_THROW(src.Update(1800.0, /*n_cells=*/5, out, names),
                    MIEM_ERROR_CATEGORY_VALIDATION,
                    MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH);
}

// ---------------------------------------------------------------------
// File pattern: `{YYYY}-{MM}.nc` + time 2024-03-15T00:00Z -> 2024-03.nc
// We verify by writing a file at that exact resolved name and observing
// that `Update` succeeds (resolving the pattern correctly).
// ---------------------------------------------------------------------
TEST(OfflineEmissionSourceTest, FilePatternTokenSubstitution)
{
  TempDir dir;
  std::vector<double> flux(6, 1.0e-9);
  // 2024-03-15T00:00:00Z = 1710460800 unix seconds.  Bracket times need
  // to surround it; use 2024-03-01 and 2024-03-31.
  CreateTestNetCDF(dir.File("2024-03.nc"),
                   /*n_times=*/2, /*n_cells=*/3,
                   /*time_values=*/{ 1709251200.0, 1711843200.0 },
                   /*species=*/{ "NOx" }, /*flux_data=*/{ flux });

  Source cfg;
  cfg.name_                   = "monthly";
  cfg.file_pattern_           = dir.Path() + "/{YYYY}-{MM}.nc";
  cfg.convention_             = "eccad";
  cfg.temporal_interpolation_ = TemporalInterpolation::Linear;
  cfg.species_map_.AddMapping("NOx", "NOx", 1.0);
  OfflineEmissionSource src(cfg);

  std::vector<Real> out;
  std::vector<std::string> names;
  EXPECT_NO_THROW(src.Update(/*time_current=*/1710460800.0, 3, out, names));
}
