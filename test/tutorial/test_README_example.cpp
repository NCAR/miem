#include <miem/miem.hpp>

#include <gtest/gtest.h>

#include <iostream>

using namespace miem;

// Mirrors the README "Using the MIEM API" example: describe one source,
// assemble it with EmissionsBuilder, and build the runtime module.  The
// builder validates the configuration in Build() (throwing
// miem::MiemException on a bad config); it does not open any file, so this
// tutorial stays file-free.
TEST(ReadmeExample, BuildsAModuleWithTheBuilder)
{
  Source cams_anthro;
  cams_anthro.name_ = "CAMS anthropogenic";
  cams_anthro.mode_ = SourceMode::Offline;
  cams_anthro.type_ = SourceType::Anthropogenic;
  cams_anthro.file_pattern_ = "/path/to/CAMS-GLOB-ANT_{YYYY}-{MM}.nc";
  cams_anthro.convention_ = "eccad";
  cams_anthro.temporal_interpolation_ = TemporalInterpolation::Linear;
  cams_anthro.vertical_injection_ = VerticalInjection::Surface;
  cams_anthro.category_ = 0;
  cams_anthro.hierarchy_ = 1;
  cams_anthro.scaling_factor_ = 1.0;
  cams_anthro.sector_ = "anthropogenic";

  // Programmatic species map: NOx -> NO (0.9), NOx -> NO2 (0.1).
  cams_anthro.species_map_.AddMapping("NOx", "NO", 0.9);
  cams_anthro.species_map_.AddMapping("NOx", "NO2", 0.1);

  // Build() throws miem::MiemException if any invariant fails.
  Emissions emissions = EmissionsBuilder()
                            .SetGridDimensions(
                                /*n_cells=*/163842,
                                /*n_vert_levels=*/60)
                            .AddSource(cams_anthro)
                            .Build();
  EXPECT_GE(emissions.NumSpecies(), 0);

  std::cout << "emissions advertises " << emissions.NumSpecies() << " mechanism species." << std::endl;
}
