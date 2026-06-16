// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Real-data integration test -- the issue #6 / #16 vehicle.
//
// Unlike test_nox_pipeline.cpp (which asserts exact algorithmic outputs on
// a synthetic file), this test runs the full Emissions pipeline against the
// real committed fixture
//   test/data/CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc
// -- a heavily-reduced but unmodified slice of CAMS-GLOB-ANT black-carbon
// emissions, already remapped onto the MPAS mesh by UPTEMPO. It exercises
// the "uptempo" convention end to end: discover the bc_anth_* fields, decode
// the xtime stamps, map bc_anth_sum -> BC, and produce per-cell surface
// flux. The fixture's true values are not known a priori, so the assertions
// are the physical ones: finite, non-negative, and not identically zero.
//
// This is the runnable counterpart of the README's "Reading a real on-mesh
// inventory" example.

#include <miem/emissions.hpp>
#include <miem/emissions_builder.hpp>
#include <miem/emissions_state.hpp>
#include <miem/source_types.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace miem;

namespace
{

  // The real fixture committed at test/data/, located via the
  // MIEM_TEST_DATA_DIR compile definition set by CMake.
  std::string RealFixturePath()
  {
    return std::string(MIEM_TEST_DATA_DIR) + "/CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc";
  }

  constexpr int kNCells = 4097;                    // cells in the subset
  constexpr double kEpoch20120101 = 1325376000.0;  // first xtime, UTC seconds

}  // namespace

// ---------------------------------------------------------------------
// Black-carbon surface flux from the real on-mesh CAMS subset.
// ---------------------------------------------------------------------
TEST(BcPipelineIntegrationTest, BlackCarbonSurfaceFluxFromRealSubset)
{
  Source cams_bc;
  cams_bc.name_ = "CAMS black carbon";
  cams_bc.mode_ = SourceMode::Offline;
  cams_bc.type_ = SourceType::Anthropogenic;
  cams_bc.file_pattern_ = RealFixturePath();
  cams_bc.convention_ = "uptempo";
  cams_bc.temporal_interpolation_ = TemporalInterpolation::Linear;
  cams_bc.vertical_injection_ = VerticalInjection::Surface;
  cams_bc.sector_ = "anthropogenic";

  // The file carries 11 BC sectors plus the precomputed bc_anth_sum; map
  // the summed total onto the mechanism's black-carbon species (identity).
  cams_bc.species_map_.AddMapping("bc_anth_sum", "BC", 1.0);

  Emissions module = EmissionsBuilder().SetGridDimensions(kNCells, /*n_vert_levels=*/1).AddSource(cams_bc).Build();

  ASSERT_EQ(module.NumSpecies(), 1);
  const auto& names = module.SpeciesNames();
  ASSERT_NE(std::find(names.begin(), names.end(), "BC"), names.end());

  // Run at the first time step in the file (2012-01-01 UTC).
  const auto state = module.Run(kEpoch20120101, /*dt=*/3600.0);

  bool any_positive = false;
  for (int ic = 0; ic < kNCells; ++ic)
  {
    const double flux = static_cast<double>(state.surface_flux_(ic, "BC"));
    EXPECT_FALSE(std::isnan(flux));
    EXPECT_GE(flux, 0.0);
    any_positive = any_positive || (flux > 0.0);
  }
  EXPECT_TRUE(any_positive);
}
