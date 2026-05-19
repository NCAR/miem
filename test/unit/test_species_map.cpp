// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for `SpeciesMap::Apply` mass-conservation cases A1-A7.
// Uses 1e-9 kg/m^2/s as the canonical "typical emission flux" magnitude
// so floating-point tolerances actually constrain — using 1.0 would
// hide precision issues per the test-writer brief.

#include <miem/species_map.hpp>
#include <miem/util/result.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace miem;

namespace {

constexpr Real kFluxMagnitude = static_cast<Real>(1.0e-9);
// Tight under double (round-off ~1e-25); relaxed under float (~1e-16).
#ifdef MIEM_USE_DOUBLE
constexpr double kAbsTol = 1.0e-22;
#else
constexpr double kAbsTol = 1.0e-15;
#endif

int IndexOf(const std::vector<std::string>& names, const std::string& s)
{
  for (int i = 0; i < static_cast<int>(names.size()); ++i)
  {
    if (names[i] == s) return i;
  }
  return -1;
}

}  // namespace

// ---------------------------------------------------------------------
// A1: NOx -> NO 0.9, NOx -> NO2 0.1 on 4 cells -> exact split, sum=1.0
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A1_NOxSplitSumsToOne)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO",  0.9);
  map.AddMapping("NOx", "NO2", 0.1);

  const int n_cells = 4;
  std::vector<Real> inv_flux(n_cells, kFluxMagnitude);

  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "NOx" }, mech_flux, n_cells);
  ASSERT_TRUE(static_cast<bool>(r))
      << (r.errors().empty() ? "" : r.errors().front().message_);

  const auto names = map.MechanismSpecies();
  const int  i_no  = IndexOf(names, "NO");
  const int  i_no2 = IndexOf(names, "NO2");
  ASSERT_GE(i_no,  0);
  ASSERT_GE(i_no2, 0);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    const Real no   = mech_flux[i_no  * n_cells + ic];
    const Real no2  = mech_flux[i_no2 * n_cells + ic];
    EXPECT_NEAR(no,  static_cast<Real>(0.9) * kFluxMagnitude, kAbsTol);
    EXPECT_NEAR(no2, static_cast<Real>(0.1) * kFluxMagnitude, kAbsTol);
    // Sum equals input (mass conservation): exact equality since
    // 0.9*x + 0.1*x algebraically equals x for the IEEE rep used here.
    EXPECT_NEAR(no + no2, kFluxMagnitude, kAbsTol);
  }
}

// ---------------------------------------------------------------------
// A2: Identity 1->1 mapping with scaling 1.0 -> output equals input
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A2_IdentityMappingPreservesInput)
{
  SpeciesMap map;
  map.AddMapping("CO", "CO", 1.0);

  const int n_cells = 3;
  std::vector<Real> inv_flux = {
      kFluxMagnitude * 1.0, kFluxMagnitude * 2.0, kFluxMagnitude * 3.0
  };

  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "CO" }, mech_flux, n_cells);
  ASSERT_TRUE(static_cast<bool>(r));

  ASSERT_EQ(mech_flux.size(), inv_flux.size());
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(mech_flux[ic], inv_flux[ic], kAbsTol);
  }
}

// ---------------------------------------------------------------------
// A3: Under-unity sum (0.5) -> output sums to 0.5 * input; remainder
// silently dropped (documented behaviour).
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A3_UnderUnitySumDropsRemainder)
{
  SpeciesMap map;
  map.AddMapping("NMVOC", "BIGALK", 0.5);

  const int n_cells = 2;
  std::vector<Real> inv_flux(n_cells, kFluxMagnitude);

  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "NMVOC" }, mech_flux, n_cells);
  ASSERT_TRUE(static_cast<bool>(r));

  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(mech_flux[ic], static_cast<Real>(0.5) * kFluxMagnitude,
                kAbsTol);
  }
}

// ---------------------------------------------------------------------
// A4: Unknown inventory species silently dropped (matches scaffolding).
// Mapping is for NOx but inventory frame supplies SO2 — output is zero.
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A4_UnknownInventorySpeciesSilentlyDropped)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO", 1.0);

  const int n_cells = 2;
  std::vector<Real> inv_flux(n_cells, kFluxMagnitude);

  // Inventory frame says these rows are SO2 — not in map.  Mapping rows
  // for NOx find no input row, so output stays zero.
  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "SO2" }, mech_flux, n_cells);
  ASSERT_TRUE(static_cast<bool>(r));

  for (auto v : mech_flux) EXPECT_EQ(v, Real{ 0 });
}

// ---------------------------------------------------------------------
// A5: Size mismatch -> CellCountMismatch
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A5_CellCountMismatchReturnsError)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO", 1.0);

  std::vector<Real> inv_flux(7, kFluxMagnitude);  // not a multiple of n_cells

  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "NOx" }, mech_flux, /*n_cells=*/4);
  EXPECT_FALSE(static_cast<bool>(r));
  ASSERT_FALSE(r.errors().empty());
  EXPECT_EQ(r.errors().front().code_, ErrorCode::CellCountMismatch);
}

// ---------------------------------------------------------------------
// A6: Two distinct inventory species mapping to same mechanism species
// -> fluxes sum.
// ---------------------------------------------------------------------
TEST(SpeciesMapApplyTest, A6_DistinctInventoryToSameMechanismSums)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO", 1.0);
  map.AddMapping("HNO3", "NO", 1.0);

  const int n_cells = 2;
  // Two rows in inventory frame: NOx (first), HNO3 (second).
  std::vector<Real> inv_flux = {
      kFluxMagnitude * 2.0, kFluxMagnitude * 2.0,   // NOx
      kFluxMagnitude * 3.0, kFluxMagnitude * 3.0    // HNO3
  };

  std::vector<Real> mech_flux;
  auto r = map.Apply(inv_flux, { "NOx", "HNO3" }, mech_flux, n_cells);
  ASSERT_TRUE(static_cast<bool>(r));

  // Both inventory rows route to NO -> NO row should equal NOx + HNO3.
  const auto names = map.MechanismSpecies();
  const int  i_no  = IndexOf(names, "NO");
  ASSERT_GE(i_no, 0);
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(mech_flux[i_no * n_cells + ic],
                static_cast<Real>(5.0) * kFluxMagnitude, kAbsTol);
  }
}

// ---------------------------------------------------------------------
// A7 (boundary): SpeciesMap::Validate boundary at 1.0 + tolerance
// (matches EmissionsConfigValidateTest cousins).
// ---------------------------------------------------------------------
TEST(SpeciesMapValidateTest, BoundaryAcceptsOnePlus1e7)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO",
                 static_cast<Real>(1.0) + static_cast<Real>(1e-7));
  auto r = map.Validate();
  EXPECT_TRUE(static_cast<bool>(r));
}

TEST(SpeciesMapValidateTest, BoundaryRejectsOnePlus1e5)
{
  SpeciesMap map;
  map.AddMapping("NOx", "NO",
                 static_cast<Real>(1.0) + static_cast<Real>(1e-5));
  auto r = map.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  ASSERT_FALSE(r.errors().empty());
  EXPECT_EQ(r.errors().front().code_, ErrorCode::SpeciesMapScalingExceedsOne);
}

// MechanismSpecies returns the cached deduplicated list.
TEST(SpeciesMapTest, MechanismSpeciesDeduplicates)
{
  SpeciesMap map;
  map.AddMapping("NOx",  "NO",  0.9);
  map.AddMapping("NOx",  "NO2", 0.1);
  map.AddMapping("HNO3", "NO",  1.0);  // same mechanism species

  auto names = map.MechanismSpecies();
  std::sort(names.begin(), names.end());
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "NO");
  EXPECT_EQ(names[1], "NO2");
}
