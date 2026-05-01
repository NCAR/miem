// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Test that the README example compiles and runs without error.

#include <miem/config.hpp>
#include <miem/emissions_module.hpp>
#include <miem/emis_state.hpp>

#include <gtest/gtest.h>

using namespace miem;

TEST(ReadmeExample, CompilesAndRuns)
{
  SourceConfig cams_anthro{
    .name_                   = "cams anthro",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Anthropogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/cams-v6.2/"
                               "CAMS-GLOB-ANT_v6.2_{YYYY}-{MM}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  SourceConfig finn_fires{
    .name_                   = "finn fires",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Fire,
    .file_pattern_           = "/glade/campaign/acom/emissions/finn-2.6/"
                               "FINN_{YYYY}{DDD}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Nearest,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 1,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "fire",
  };

  SourceConfig megan_offline_climo{
    .name_                   = "megan offline climo",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Biogenic,
    .file_pattern_           = "/glade/campaign/acom/emissions/megan-climo/"
                               "MEGAN_climo_{MM}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 2,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "biogenic",
  };

  MIEMConfig cfg{
    .version_ = "1.0.0",
    .sources_ = { cams_anthro, finn_fires, megan_offline_climo },
  };

  const int n_cells       = 163842;
  const int n_vert_levels = 60;
  EmissionsModule module(cfg, n_cells, n_vert_levels);

  const double sim_time_sec = 86400.0 * 180.0;
  const double dt_sec       = 600.0;
  EmisState state = module.Run(sim_time_sec, dt_sec);

  // Stub returns zero fluxes — verify the API runs without error
  EXPECT_EQ(state.surface_flux_(0, "NO"), 0.0);
  EXPECT_EQ(state.surface_flux_(0, "CO"), 0.0);
  EXPECT_EQ(state.surface_flux_(0, "ISOP"), 0.0);

  EXPECT_TRUE(state.sector_fluxes_.count("anthropogenic") > 0);
  EXPECT_TRUE(state.sector_fluxes_.count("fire") > 0);
  EXPECT_TRUE(state.sector_fluxes_.count("biogenic") > 0);
}
