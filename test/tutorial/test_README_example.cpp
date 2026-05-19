#include <miem/config.hpp>
#include <miem/emissions_state.hpp>
#include <miem/emissions.hpp>

#include <gtest/gtest.h>
#include <iostream>

using namespace miem;

TEST(ReadmeExample, BuildsAConfigAndModule)
{
  SourceConfig cams_anthro;
  cams_anthro.name_                   = "CAMS anthropogenic";
  cams_anthro.mode_                   = SourceMode::Offline;
  cams_anthro.type_                   = SourceType::Anthropogenic;
  cams_anthro.file_pattern_           = "/path/to/CAMS-GLOB-ANT_{YYYY}-{MM}.nc";
  cams_anthro.convention_             = "eccad";
  cams_anthro.temporal_interpolation_ = TemporalInterpolation::Linear;
  cams_anthro.vertical_injection_     = VerticalInjection::Surface;
  cams_anthro.category_               = 0;
  cams_anthro.hierarchy_              = 1;
  cams_anthro.scaling_factor_         = 1.0;
  cams_anthro.sector_                 = "anthropogenic";

  // Programmatic species map: NOx -> NO (0.9), NOx -> NO2 (0.1).
  cams_anthro.species_map_.AddMapping("NOx", "NO",  0.9);
  cams_anthro.species_map_.AddMapping("NOx", "NO2", 0.1);

  MIEMConfig cfg;
  cfg.sources_  = { cams_anthro };

  // The config alone is valid prior to opening any file — that is what
  // this tutorial covers.  File-touching tests live elsewhere.
  auto valid = cfg.Validate();
  ASSERT_TRUE(static_cast<bool>(valid))
      << "config failed to validate: "
      << (valid.errors().empty() ? "" : valid.errors().front().message_);

  Emissions module(cfg, /*n_cells=*/163842, /*n_vert_levels=*/60);
  EXPECT_GE(module.NumSpecies(), 0);

  std::cout << "MIEMConfig validated; module exposes "
            << module.NumSpecies() << " mechanism species." << std::endl;
}
