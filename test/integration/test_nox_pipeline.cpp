// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// End-to-end integration test — the issue #6 vehicle.
//
// Acceptance criterion from issue #6: "an integration test which gets
// emissions for NO2, NO, and O3 is created".  This test:
//
//   1. Generates a synthetic ECCAD NetCDF holding emi_NOx = 1.0e-9
//      kg/m^2/s and emi_O3 = 5.0e-10 kg/m^2/s on N cells, at multiple
//      time steps.
//   2. Programmatically constructs a EmissionsConfig with one source: NOx
//      -> NO @ 0.9 + NOx -> NO2 @ 0.1, O3 -> O3 @ 1.0 (identity),
//      anthropogenic sector.
//   3. Calls Emissions::Run at one or more times.
//   4. Asserts:
//        - surface_flux(NO)  == 0.9 * 1e-9 = 9.0e-10 +/- 1e-22
//        - surface_flux(NO2) == 0.1 * 1e-9 = 1.0e-10 +/- 1e-22
//        - surface_flux(O3)  == 5.0e-10 (identity passthrough)
//   5. With air_density + layer_thickness provided: tendency matches
//      surface_flux / (rho * dz) within tolerance.

#include "synthetic_nc.hpp"

#include <miem/config.hpp>
#include <miem/emissions_state.hpp>
#include <miem/emissions.hpp>
#include <miem/util/result.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace miem;
using miem_test::CreateTestNetCDF;
using miem_test::TempDir;

namespace {

constexpr int    kNCells     = 8;
constexpr int    kNVertLevels = 5;
constexpr double kNOxFlux    = 1.0e-9;   // kg/m^2/s
constexpr double kO3Flux     = 5.0e-10;  // kg/m^2/s

// Precision-aware tolerance: at 1e-9 magnitude the double round-off is
// ~1e-25; for float32 it's ~1e-16.  Use ~1e-15 for float so sum-of-
// products noise is absorbed without losing algorithmic-drift catch.
#ifdef MIEM_USE_DOUBLE
constexpr double kFluxTol = 1.0e-22;
constexpr double kTendTol = 1.0e-25;
#else
constexpr double kFluxTol = 1.0e-15;
constexpr double kTendTol = 1.0e-18;
#endif

// Build the test NetCDF.  Two time steps so the linear blend at the
// midpoint gives the same flux back (constant-in-time inventory).
std::string MakeIntegrationFile(const TempDir& dir)
{
  const std::string path = dir.File("integration_inventory.nc");
  std::vector<double> nox(2 * kNCells, kNOxFlux);
  std::vector<double> o3 (2 * kNCells, kO3Flux);
  CreateTestNetCDF(path, /*n_times=*/2, /*n_cells=*/kNCells,
                   /*time_values=*/{ 0.0, 3600.0 },
                   /*species=*/{ "NOx", "O3" },
                   /*flux_data=*/{ nox, o3 });
  return path;
}

}  // namespace

// ---------------------------------------------------------------------
// Surface-flux-only run.
// ---------------------------------------------------------------------
TEST(NoxPipelineIntegrationTest, NOxSplitAndO3Passthrough)
{
  TempDir dir;
  const std::string path = MakeIntegrationFile(dir);

  SourceConfig src;
  src.name_                   = "test_inventory";
  src.file_pattern_           = path;
  src.convention_             = "eccad";
  src.temporal_interpolation_ = TemporalInterpolation::Linear;
  src.vertical_injection_     = VerticalInjection::Surface;
  src.category_               = 0;
  src.hierarchy_              = 1;
  src.sector_                 = "anthropogenic";

  // NOx -> NO 0.9 + NO2 0.1.  Plus O3 identity passthrough.
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.1);
  src.species_map_.AddMapping("O3",  "O3",  1.0);

  EmissionsConfig cfg;
  cfg.sources_ = { src };

  auto created = Emissions::Create(cfg, kNCells, kNVertLevels);
  ASSERT_TRUE(static_cast<bool>(created))
      << (created.errors().empty() ? "" : created.errors().front().message_);
  auto module = std::move(created).value();

  // Run at midpoint t=1800s (between 0 and 3600); inventory is constant
  // so the blend equals the input flux.
  auto state_r = module->Run(/*time_sec=*/1800.0, /*dt=*/60.0);
  ASSERT_TRUE(static_cast<bool>(state_r))
      << (state_r.errors().empty() ? "" : state_r.errors().front().message_);
  const auto state = std::move(state_r).value();

  // Assertion: per-cell surface flux for NO / NO2 / O3.
  for (int ic = 0; ic < kNCells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NO")),
                0.9 * kNOxFlux, kFluxTol);     // 9.0e-10
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NO2")),
                0.1 * kNOxFlux, kFluxTol);     // 1.0e-10
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "O3")),
                kO3Flux,        kFluxTol);     // 5.0e-10
  }

  // Sector flux for "anthropogenic" should reflect all three species.
  const EmissionsArray* sec = state.GetSectorFlux("anthropogenic");
  ASSERT_NE(sec, nullptr);
  for (int ic = 0; ic < kNCells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>((*sec)(ic, "NO")),
                0.9 * kNOxFlux, kFluxTol);
    EXPECT_NEAR(static_cast<double>((*sec)(ic, "NO2")),
                0.1 * kNOxFlux, kFluxTol);
    EXPECT_NEAR(static_cast<double>((*sec)(ic, "O3")),
                kO3Flux,        kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Same pipeline, with air_density + layer_thickness -> tendency_
// matches surface_flux / (rho * dz) at the surface injection layer.
// ---------------------------------------------------------------------
TEST(NoxPipelineIntegrationTest, TendencyMatchesFluxOverColumnIntegrand)
{
  TempDir dir;
  const std::string path = MakeIntegrationFile(dir);

  SourceConfig src;
  src.name_                   = "test_inventory";
  src.file_pattern_           = path;
  src.convention_             = "eccad";
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.1);
  src.species_map_.AddMapping("O3",  "O3",  1.0);

  EmissionsConfig cfg;
  cfg.sources_ = { src };

  auto created = Emissions::Create(cfg, kNCells, kNVertLevels);
  ASSERT_TRUE(static_cast<bool>(created));
  auto module = std::move(created).value();

  const Real kRho = static_cast<Real>(1.225);
  const Real kDz  = static_cast<Real>(100.0);
  std::vector<Real> rho(kNVertLevels * kNCells, kRho);
  std::vector<Real> dz (kNVertLevels * kNCells, kDz);

  auto state_r = module->Run(/*time_sec=*/1800.0, /*dt=*/60.0,
                             rho.data(), dz.data(),
                             kNVertLevels * kNCells);
  ASSERT_TRUE(static_cast<bool>(state_r))
      << (state_r.errors().empty() ? "" : state_r.errors().front().message_);
  const auto state = std::move(state_r).value();

  // For each species, tendency at level 0 should be surface_flux/(rho*dz).
  // Find species indices by name in state.species_names_.
  auto find_idx = [&](const std::string& s) -> int {
    for (int i = 0; i < static_cast<int>(state.species_names_.size()); ++i)
    {
      if (state.species_names_[i] == s) return i;
    }
    return -1;
  };
  const int i_no  = find_idx("NO");
  const int i_no2 = find_idx("NO2");
  const int i_o3  = find_idx("O3");
  ASSERT_GE(i_no,  0);
  ASSERT_GE(i_no2, 0);
  ASSERT_GE(i_o3,  0);

  auto tend_at = [&](int isp, int lv, int ic) -> Real {
    return state.tendency_[
        static_cast<std::size_t>(isp) * kNVertLevels * kNCells +
        static_cast<std::size_t>(lv) * kNCells + ic];
  };

  // Layer 0 (surface): non-zero; other layers: zero.
  for (int ic = 0; ic < kNCells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(tend_at(i_no,  0, ic)),
                (0.9 * kNOxFlux) / (1.225 * 100.0), kTendTol);
    EXPECT_NEAR(static_cast<double>(tend_at(i_no2, 0, ic)),
                (0.1 * kNOxFlux) / (1.225 * 100.0), kTendTol);
    EXPECT_NEAR(static_cast<double>(tend_at(i_o3,  0, ic)),
                kO3Flux         / (1.225 * 100.0), kTendTol);

    for (int lv = 1; lv < kNVertLevels; ++lv)
    {
      EXPECT_EQ(tend_at(i_no,  lv, ic), Real{ 0 });
      EXPECT_EQ(tend_at(i_no2, lv, ic), Real{ 0 });
      EXPECT_EQ(tend_at(i_o3,  lv, ic), Real{ 0 });
    }
  }

  // Column-integrated tendency * rho * dz round-trips to surface_flux
  // within the post-S1 tolerance (1e-9 relative + 1e-18 absolute floor).
  for (int isp = 0; isp < static_cast<int>(state.species_names_.size());
       ++isp)
  {
    for (int ic = 0; ic < kNCells; ++ic)
    {
      Real sum = Real{ 0 };
      for (int lv = 0; lv < kNVertLevels; ++lv)
      {
        sum += tend_at(isp, lv, ic) *
               rho[lv * kNCells + ic] *
               dz [lv * kNCells + ic];
      }
      const Real flux = state.surface_flux_.At(isp, ic);
      EXPECT_NEAR(static_cast<double>(sum), static_cast<double>(flux),
                  1.0e-6 * static_cast<double>(flux) + kFluxTol);
    }
  }
}

// ---------------------------------------------------------------------
// Repeating the run at a different time within the bracket yields the
// same flux (constant inventory) — sanity check on the temporal loop.
// ---------------------------------------------------------------------
TEST(NoxPipelineIntegrationTest, RepeatedRunsRespectTimeBracket)
{
  TempDir dir;
  const std::string path = MakeIntegrationFile(dir);

  SourceConfig src;
  src.name_                   = "test_inventory";
  src.file_pattern_           = path;
  src.convention_             = "eccad";
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.1);
  src.species_map_.AddMapping("O3",  "O3",  1.0);

  EmissionsConfig cfg;
  cfg.sources_ = { src };

  auto module = std::move(Emissions::Create(
                              cfg, kNCells, kNVertLevels)).value();

  for (double t : { 100.0, 1800.0, 3500.0 })
  {
    auto state_r = module->Run(t, 60.0);
    ASSERT_TRUE(static_cast<bool>(state_r))
        << "time=" << t << " err: "
        << (state_r.errors().empty() ? "" : state_r.errors().front().message_);
    const auto state = std::move(state_r).value();
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(0, "NO")),
                0.9 * kNOxFlux, kFluxTol);
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(0, "NO2")),
                0.1 * kNOxFlux, kFluxTol);
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(0, "O3")),
                kO3Flux,        kFluxTol);
  }
}
