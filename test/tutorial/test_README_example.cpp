#include <miem/config.hpp>
#include <miem/emissions_module.hpp>
#include <miem/emis_state.hpp>

#include <gtest/gtest.h>
#include <iostream>

using namespace miem;

TEST(ReadmeExample, RunsWithoutError)
{
  SourceConfig cams_anthro{
    .name_                   = "CAMS anthropogenic",
    .mode_                   = SourceMode::Offline,
    .type_                   = SourceType::Anthropogenic,
    .file_pattern_           = "/path/to/CAMS-GLOB-ANT_{YYYY}-{MM}.nc",
    .temporal_interpolation_ = TemporalInterpolation::Linear,
    .vertical_injection_     = VerticalInjection::Surface,
    .category_               = 0,
    .hierarchy_              = 1,
    .scaling_factor_         = 1.0,
    .sector_                 = "anthropogenic",
  };

  MIEMConfig cfg{
    .version_ = "1.0.0",
    .sources_ = { cams_anthro },
  };

  EmissionsModule module(cfg, /*n_cells=*/163842, /*n_vert_levels=*/60);

  EmisState state = module.Run(
    86400.0 * 180.0,  // sim_time_sec: day 180
    600.0             // dt_sec: 10 minutes
  );

  std::cout << "NO surface flux at cell 0: "
            << state.surface_flux_(0, "NO")
            << " kg m-2 s-1" << std::endl;
}
