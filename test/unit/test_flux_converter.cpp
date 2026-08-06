// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for `FluxConverter`.  Reference value, zero-density guard,
// multi-cell/level surface injection, and column-integral mass
// conservation round-trip.

#include <miem/emissions_state.hpp>
#include <miem/flux_converter.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace miem;

namespace
{

// Precision-aware tolerances.  Tight under double (round-off ~1e-25 at
// the tendency magnitude 1e-11); relaxed under float (~1e-18).
#ifdef MIEM_USE_DOUBLE
  constexpr double kTendTol = 1.0e-25;
  constexpr double kFluxTol = 1.0e-22;
#else
  constexpr double kTendTol = 1.0e-18;
  constexpr double kFluxTol = 1.0e-15;
#endif

  // Build an EmissionsState pre-populated with a single species + surface flux.
  EmissionsState MakeState(int n_cells, int n_vert_levels, Real flux_value)
  {
    EmissionsState s(/*n_species=*/1, n_cells, n_vert_levels);
    s.species_names_ = { "NO" };
    s.injection_layer_ = { 0 };
    s.surface_flux_.SetSpecies(s.species_names_);
    for (int ic = 0; ic < n_cells; ++ic)
    {
      s.surface_flux_.At(0, ic) = flux_value;
    }
    return s;
  }

}  // namespace

// ---------------------------------------------------------------------
// Reference value: flux=1e-9, rho=1.225, dz=100 -> 8.16326...e-12
// (exact value: 1e-9 / (1.225 * 100) = 8.163265306122449e-12)
// ---------------------------------------------------------------------
TEST(FluxConverterTest, ReferenceValueScalar)
{
  const Real flux = static_cast<Real>(1.0e-9);
  const Real rho = static_cast<Real>(1.225);
  const Real dz = static_cast<Real>(100.0);

  const Real tend = FluxConverter::FluxToTendency(flux, rho, dz);
  EXPECT_NEAR(static_cast<double>(tend), 8.163265306122449e-12, kTendTol);
}

// ---------------------------------------------------------------------
// Zero air density -> tendency = 0 (NOT NaN)
// ---------------------------------------------------------------------
TEST(FluxConverterTest, ZeroDensityYieldsZeroTendency)
{
  const Real flux = static_cast<Real>(1.0e-9);
  const Real rho = Real{ 0 };
  const Real dz = static_cast<Real>(100.0);

  const Real tend = FluxConverter::FluxToTendency(flux, rho, dz);
  EXPECT_EQ(tend, Real{ 0 });
  EXPECT_FALSE(std::isnan(static_cast<double>(tend)));
}

TEST(FluxConverterTest, ZeroLayerThicknessYieldsZeroTendency)
{
  const Real tend = FluxConverter::FluxToTendency(static_cast<Real>(1.0e-9), static_cast<Real>(1.225), Real{ 0 });
  EXPECT_EQ(tend, Real{ 0 });
}

// ---------------------------------------------------------------------
// Multi-cell, multi-layer surface injection: tendency only at
// injection_layer_; other levels stay zero.
// ---------------------------------------------------------------------
TEST(FluxConverterTest, SurfaceInjectionOnlyAtLayerZero)
{
  const int n_cells = 4;
  const int n_vl = 5;
  const Real flux = static_cast<Real>(1.0e-9);

  EmissionsState s = MakeState(n_cells, n_vl, flux);

  std::vector<Real> rho(n_vl * n_cells, static_cast<Real>(1.0));
  std::vector<Real> dz(n_vl * n_cells, static_cast<Real>(100.0));

  ASSERT_NO_THROW(FluxConverter::Apply(s, rho.data(), dz.data(), n_vl * n_cells));

  const std::size_t n_vl_x_cells = static_cast<std::size_t>(n_vl) * n_cells;
  ASSERT_EQ(s.tendency_.size(), 1u * n_vl_x_cells);

  for (int ic = 0; ic < n_cells; ++ic)
  {
    // Layer 0 -> non-zero (flux / (rho*dz) = 1e-9/100 = 1e-11)
    const Real expect = flux / (static_cast<Real>(1.0) * static_cast<Real>(100.0));
    const std::size_t idx0 = 0u * n_cells + ic;
    EXPECT_NEAR(static_cast<double>(s.tendency_[idx0]), static_cast<double>(expect), kTendTol);

    // All other layers -> exactly zero
    for (int lv = 1; lv < n_vl; ++lv)
    {
      const std::size_t idx = static_cast<std::size_t>(lv) * n_cells + ic;
      EXPECT_EQ(s.tendency_[idx], Real{ 0 });
    }
  }
}

// ---------------------------------------------------------------------
// Column-integrated tendency * rho * dz = surface_flux within
// 1e-9 * |flux| tolerance (mass conservation round-trip; S1 fix test).
// ---------------------------------------------------------------------
TEST(FluxConverterTest, ColumnIntegralMatchesSurfaceFlux)
{
  const int n_cells = 4;
  const int n_vl = 3;
  const Real flux = static_cast<Real>(1.0e-9);

  EmissionsState s = MakeState(n_cells, n_vl, flux);
  std::vector<Real> rho(n_vl * n_cells, static_cast<Real>(1.225));
  std::vector<Real> dz(n_vl * n_cells, static_cast<Real>(100.0));

  ASSERT_NO_THROW(FluxConverter::Apply(s, rho.data(), dz.data(), n_vl * n_cells));

  for (int ic = 0; ic < n_cells; ++ic)
  {
    Real sum = Real{ 0 };
    for (int lv = 0; lv < n_vl; ++lv)
    {
      const std::size_t idx = static_cast<std::size_t>(lv) * n_cells + ic;
      sum += s.tendency_[idx] * rho[lv * n_cells + ic] * dz[lv * n_cells + ic];
    }
    // Post-S1 effective tolerance: 1e-9 relative under double,
    // ~1e-6 under float to absorb float32 sum-of-products noise.
#ifdef MIEM_USE_DOUBLE
    const double rel = 1.0e-9;
#else
    const double rel = 1.0e-6;
#endif
    EXPECT_NEAR(static_cast<double>(sum), static_cast<double>(flux), rel * static_cast<double>(flux) + kFluxTol);
  }
}

TEST(FluxConverterTest, ProfileLayerFluxConvertsEveryLevelAndClosesMass)
{
  const int n_cells = 2;
  const int n_vl = 3;
  const Real column_flux = static_cast<Real>(8.0e-9);
  EmissionsState state = MakeState(n_cells, n_vl, column_flux);
  state.has_layer_flux_ = true;
  const std::vector<Real> fractions = { Real{ 0 }, static_cast<Real>(0.25), static_cast<Real>(0.75) };
  for (int level = 0; level < n_vl; ++level)
  {
    for (int cell = 0; cell < n_cells; ++cell)
    {
      state.layer_flux_[static_cast<std::size_t>(level) * n_cells + cell] = column_flux * fractions[level];
    }
  }
  std::vector<Real> rho = {
    static_cast<Real>(1.2), static_cast<Real>(1.1),
    static_cast<Real>(1.0), static_cast<Real>(0.9),
    static_cast<Real>(0.8), static_cast<Real>(0.7),
  };
  std::vector<Real> dz = {
    static_cast<Real>(100.0), static_cast<Real>(110.0),
    static_cast<Real>(120.0), static_cast<Real>(130.0),
    static_cast<Real>(140.0), static_cast<Real>(150.0),
  };

  ASSERT_NO_THROW(FluxConverter::Apply(state, rho.data(), dz.data(), n_vl * n_cells));
  for (int cell = 0; cell < n_cells; ++cell)
  {
    Real reconstructed = Real{ 0 };
    for (int level = 0; level < n_vl; ++level)
    {
      const std::size_t index = static_cast<std::size_t>(level) * n_cells + cell;
      const Real expected = FluxConverter::FluxToTendency(state.layer_flux_[index], rho[index], dz[index]);
      EXPECT_NEAR(static_cast<double>(state.tendency_[index]), static_cast<double>(expected), kTendTol);
      reconstructed += state.tendency_[index] * rho[index] * dz[index];
    }
    EXPECT_NEAR(static_cast<double>(reconstructed), static_cast<double>(column_flux), kFluxTol);
  }
}

TEST(FluxConverterTest, MalformedLayerFluxSizeThrows)
{
  EmissionsState state = MakeState(2, 3, static_cast<Real>(1.0e-9));
  state.has_layer_flux_ = true;
  state.layer_flux_.pop_back();
  std::vector<Real> rho(6, Real{ 1 });
  std::vector<Real> dz(6, static_cast<Real>(100.0));
  EXPECT_THROW(FluxConverter::Apply(state, rho.data(), dz.data(), 6), MiemException);
}

// ---------------------------------------------------------------------
// Cell-count mismatch: too few rho/dz elements -> throws.
// ---------------------------------------------------------------------
TEST(FluxConverterTest, InsufficientAtmElementsThrows)
{
  const int n_cells = 4;
  const int n_vl = 3;
  EmissionsState s = MakeState(n_cells, n_vl, static_cast<Real>(1.0e-9));

  std::vector<Real> rho(2, static_cast<Real>(1.0));  // far too short
  std::vector<Real> dz(2, static_cast<Real>(100.0));

  EXPECT_THROW(
      {
        try
        {
          FluxConverter::Apply(
              s,
              rho.data(),
              dz.data(),
              /*n_atm_elements=*/2);
        }
        catch (const miem::MiemException& e)
        {
          EXPECT_STREQ(e.category_, MIEM_ERROR_CATEGORY_VALIDATION);
          EXPECT_EQ(e.code_, MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH);
          throw;
        }
      },
      miem::MiemException);
}
