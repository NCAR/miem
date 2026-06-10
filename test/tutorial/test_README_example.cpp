#include <miem/source_types.hpp>
#include <miem/emissions.hpp>
#include <miem/emissions_state.hpp>

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

using namespace miem;

// At this stage of the port MIEM ships the `Source` description type; the
// fluent `EmissionsBuilder` that assembles sources into a runtime module
// lands with the runtime-integration slice.  This tutorial builds a single
// source and hands it to the interim module placeholder.
TEST(ReadmeExample, BuildsASource)
{
  Source cams_anthro{
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

  // Programmatic species map: NOx -> NO (0.9), NOx -> NO2 (0.1).
  cams_anthro.species_map_.AddMapping("NOx", "NO",  0.9);
  cams_anthro.species_map_.AddMapping("NOx", "NO2", 0.1);

  Emissions emissions(std::vector<Source>{ cams_anthro },
                      /*n_cells=*/163842, /*n_vert_levels=*/60);

  EmissionsState state = emissions.Run(
    86400.0 * 180.0,  // sim_time_sec: day 180
    600.0             // dt_sec: 10 minutes
  );

  std::cout << "module holds " << emissions.NumSources()
            << " emission source(s)." << std::endl;
}
