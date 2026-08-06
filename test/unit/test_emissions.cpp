// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Emissions runtime tests: HEMCO category/hierarchy aggregation,
// per-source scaling, and per-sector diagnostics.  Modules are assembled
// with EmissionsBuilder.  Uses synthetic NetCDF files because
// OfflineEmissionSource is the only v1 source path.

#include "synthetic_nc.hpp"

#include <miem/emissions.hpp>
#include <miem/emissions_builder.hpp>
#include <miem/emissions_state.hpp>
#include <miem/source_types.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>

using namespace miem;
using miem_test::CreateTestNetCDF;
using miem_test::TempDir;

namespace
{

  // Precision-aware tolerance: tight under double, relaxed under float.
  constexpr double kFluxTol = std::is_same_v<miem::Real, double> ? 1.0e-22 : 1.0e-15;

  // Helper: write a single-species two-time-step file (constant in time).
  std::string
  MakeFile(const TempDir& dir, const std::string& name, int n_cells, double flux_value, const std::string& species = "NOx")
  {
    const std::string path = dir.File(name);
    std::vector<double> data(2 * n_cells, flux_value);
    CreateTestNetCDF(
        path,
        /*n_times=*/2,
        n_cells,
        /*time_values=*/{ 0.0, 3600.0 },
        /*species=*/{ species },
        /*flux_data=*/{ data });
    return path;
  }

  Source MakeSource(
      const std::string& name,
      const std::string& path,
      int category,
      int hierarchy,
      const std::string& sector = "",
      double scaling = 1.0)
  {
    Source src;
    src.name_ = name;
    src.file_pattern_ = path;
    src.convention_ = "eccad";
    src.category_ = category;
    src.hierarchy_ = hierarchy;
    src.sector_ = sector;
    src.scaling_factor_ = static_cast<Real>(scaling);
    src.species_map_.AddMapping("NOx", "NOx", 1.0);
    return src;
  }

}  // namespace

// ---------------------------------------------------------------------
// E1 / surface_flux sums when sources are in different categories.
// ---------------------------------------------------------------------
TEST(EmissionsTest, DifferentCategoriesSum)
{
  TempDir dir;
  const int n_cells = 4;
  const std::string p1 = MakeFile(dir, "a.nc", n_cells, 1.0e-9);
  const std::string p2 = MakeFile(dir, "b.nc", n_cells, 2.0e-9);

  Emissions module = EmissionsBuilder()
                         .SetGridDimensions(n_cells, /*n_vl=*/2)
                         .AddSource(MakeSource("A", p1, /*cat=*/0, /*hier=*/1))
                         .AddSource(MakeSource("B", p2, /*cat=*/1, /*hier=*/1))
                         .Build();

  const auto state = module.Run(/*time_sec=*/1800.0, /*dt=*/60.0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NOx")), 3.0e-9, kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Same category, B higher hierarchy, both nonzero -> B shadows A.
// ---------------------------------------------------------------------
TEST(EmissionsTest, SameCategoryHigherHierarchyShadows)
{
  TempDir dir;
  const int n_cells = 4;
  const std::string p_low = MakeFile(dir, "low.nc", n_cells, 5.0e-9);
  const std::string p_high = MakeFile(dir, "high.nc", n_cells, 9.0e-9);

  Emissions module = EmissionsBuilder()
                         .SetGridDimensions(n_cells, /*n_vl=*/2)
                         .AddSource(MakeSource("low", p_low, /*cat=*/0, /*hier=*/1))
                         .AddSource(MakeSource("high", p_high, /*cat=*/0, /*hier=*/2))
                         .Build();

  const auto state = module.Run(1800.0, 60.0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NOx")), 9.0e-9, kFluxTol);  // high wins; low does NOT add
  }
}

// ---------------------------------------------------------------------
// Same category, B higher hierarchy, B has bit-exact 0 -> A wins.
// ---------------------------------------------------------------------
TEST(EmissionsTest, FallThroughWhenHigherHierarchyIsBitExactZero)
{
  TempDir dir;
  const int n_cells = 4;
  const std::string p_low = MakeFile(dir, "low.nc", n_cells, 5.0e-9);
  const std::string p_high = MakeFile(dir, "high.nc", n_cells, 0.0);  // bit-exact

  Emissions module = EmissionsBuilder()
                         .SetGridDimensions(n_cells, /*n_vl=*/2)
                         .AddSource(MakeSource("low", p_low, /*cat=*/0, /*hier=*/1))
                         .AddSource(MakeSource("high", p_high, /*cat=*/0, /*hier=*/2))
                         .Build();

  const auto state = module.Run(1800.0, 60.0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NOx")), 5.0e-9, kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Per-source scaling_factor applied before hierarchy resolution:
// A alone, scaling=0.5, flux=2e-9 -> surface = 1e-9.
// ---------------------------------------------------------------------
TEST(EmissionsTest, PerSourceScalingAppliedBeforeHierarchy)
{
  TempDir dir;
  const int n_cells = 4;
  const std::string path = MakeFile(dir, "a.nc", n_cells, 2.0e-9);

  Emissions module = EmissionsBuilder()
                         .SetGridDimensions(n_cells, 2)
                         .AddSource(MakeSource("A", path, 0, 1, /*sector=*/"", /*scaling=*/0.5))
                         .Build();

  const auto state = module.Run(1800.0, 60.0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NOx")), 1.0e-9, kFluxTol);
  }
}

TEST(EmissionsTest, SelectedModeIsBitwiseEqualToFullGrid)
{
  TempDir dir;
  const int n_cells = 6;
  const std::string path = dir.File("selected.nc");
  const std::vector<double> data = {
    1.0e-9, 2.0e-9, 3.0e-9, 4.0e-9, 5.0e-9, 6.0e-9,
    2.0e-9, 4.0e-9, 6.0e-9, 8.0e-9, 10.0e-9, 12.0e-9,
  };
  CreateTestNetCDF(path, 2, n_cells, { 0.0, 3600.0 }, { "NOx" }, { data });

  Emissions full = EmissionsBuilder()
                       .SetGridDimensions(n_cells, 2)
                       .AddSource(MakeSource("source", path, 0, 1))
                       .Build();
  const std::vector<int> selected_ids = { 6, 2, 3 };
  Emissions selected = EmissionsBuilder()
                           .SetGridDimensions(n_cells, 2)
                           .SetCellSelection(selected_ids)
                           .AddSource(MakeSource("source", path, 0, 1))
                           .Build();

  const auto full_state = full.Run(1800.0, 60.0);
  const auto selected_state = selected.Run(1800.0, 60.0);
  ASSERT_EQ(selected_state.n_cells_, static_cast<int>(selected_ids.size()));
  for (std::size_t i = 0; i < selected_ids.size(); ++i)
  {
    EXPECT_EQ(
        selected_state.surface_flux_(static_cast<int>(i), "NOx"),
        full_state.surface_flux_(selected_ids[i] - 1, "NOx"));
  }
}

// ---------------------------------------------------------------------
// Sector fluxes are pre-hierarchy: both shadowing sources record their
// contributions independently in sector_fluxes_ even when one shadows
// the other in surface_flux_.
// ---------------------------------------------------------------------
TEST(EmissionsTest, SectorFluxesPreHierarchy)
{
  TempDir dir;
  const int n_cells = 3;
  const std::string p_low = MakeFile(dir, "low.nc", n_cells, 1.0e-9);
  const std::string p_high = MakeFile(dir, "high.nc", n_cells, 7.0e-9);

  Emissions module = EmissionsBuilder()
                         .SetGridDimensions(n_cells, 2)
                         .AddSource(MakeSource("low", p_low, 0, 1, /*sector=*/"anthropogenic"))
                         .AddSource(MakeSource("high", p_high, 0, 2, /*sector=*/"anthropogenic"))
                         .Build();

  const auto state = module.Run(1800.0, 60.0);

  // surface_flux_ reflects hierarchy winner (7e-9).
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>(state.surface_flux_(ic, "NOx")), 7.0e-9, kFluxTol);
  }

  // sector_fluxes_ aggregates both sources (1e-9 + 7e-9 = 8e-9).
  const EmissionsArray* anthro = state.GetSectorFlux("anthropogenic");
  ASSERT_NE(anthro, nullptr);
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(static_cast<double>((*anthro)(ic, "NOx")), 8.0e-9, kFluxTol);
  }
}

// ---------------------------------------------------------------------
// Source-ordering commutativity: swapping the source list order yields
// the same surface flux.
// ---------------------------------------------------------------------
TEST(EmissionsTest, OrderingDoesNotAffectOutput)
{
  TempDir dir;
  const int n_cells = 3;
  const std::string p1 = MakeFile(dir, "a.nc", n_cells, 1.0e-9);
  const std::string p2 = MakeFile(dir, "b.nc", n_cells, 2.0e-9);

  Emissions m1 = EmissionsBuilder()
                     .SetGridDimensions(n_cells, 2)
                     .AddSource(MakeSource("A", p1, 0, 1))
                     .AddSource(MakeSource("B", p2, 1, 1))
                     .Build();

  Emissions m2 = EmissionsBuilder()
                     .SetGridDimensions(n_cells, 2)
                     .AddSource(MakeSource("B", p2, 1, 1))
                     .AddSource(MakeSource("A", p1, 0, 1))
                     .Build();

  const auto s1 = m1.Run(1800.0, 60.0);
  const auto s2 = m2.Run(1800.0, 60.0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(
        static_cast<double>(s1.surface_flux_(ic, "NOx")), static_cast<double>(s2.surface_flux_(ic, "NOx")), kFluxTol);
  }
}
