#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "miem/flux_converter.hpp"

using namespace miem;

TEST(FluxConverter, SinglePointConversion) {
  // flux = 1e-6 kg/m^2/s, rho = 1.225 kg/m^3, dz = 100 m
  // tendency = 1e-6 / (1.225 * 100) = 8.163e-9 kg/kg/s
  Real tendency = FluxConverter::FluxToTendency(1e-6, 1.225, 100.0);
  EXPECT_NEAR(tendency, 1e-6 / (1.225 * 100.0), 1e-15);
}

TEST(FluxConverter, ZeroDensityReturnZero) {
  EXPECT_DOUBLE_EQ(FluxConverter::FluxToTendency(1e-6, 0.0, 100.0), 0.0);
}

TEST(FluxConverter, ZeroThicknessReturnZero) {
  EXPECT_DOUBLE_EQ(FluxConverter::FluxToTendency(1e-6, 1.225, 0.0), 0.0);
}

TEST(FluxConverter, ZeroFluxReturnZero) {
  EXPECT_DOUBLE_EQ(FluxConverter::FluxToTendency(0.0, 1.225, 100.0), 0.0);
}

TEST(FluxConverter, ConvertEmisState) {
  const int n_species = 2;
  const int n_cells = 3;
  const int n_vert_levels = 2;

  EmisState state(n_species, n_cells, n_vert_levels);

  // Set surface fluxes
  for (int isp = 0; isp < n_species; ++isp) {
    for (int ic = 0; ic < n_cells; ++ic) {
      state.surface_flux[isp * n_cells + ic] = 1e-6 * (isp + 1);
    }
  }

  // All species inject at surface (layer 0, default)
  state.injection_layer = {0, 0};

  // Set atmospheric state
  std::vector<Real> air_density(n_vert_levels * n_cells);
  std::vector<Real> layer_thickness(n_vert_levels * n_cells);
  for (int k = 0; k < n_vert_levels; ++k) {
    for (int ic = 0; ic < n_cells; ++ic) {
      air_density[k * n_cells + ic] = 1.225 - k * 0.1;
      layer_thickness[k * n_cells + ic] = 100.0 + k * 50.0;
    }
  }

  FluxConverter::Convert(state, air_density.data(), layer_thickness.data(),
                         static_cast<int>(air_density.size()));

  // Check tendency at surface layer (layer 0) for species 0
  for (int ic = 0; ic < n_cells; ++ic) {
    Real flux = 1e-6;
    Real rho = air_density[0 * n_cells + ic];
    Real dz = layer_thickness[0 * n_cells + ic];
    Real expected = flux / (rho * dz);

    size_t tend_idx = 0 * n_vert_levels * n_cells + 0 * n_cells + ic;
    EXPECT_NEAR(state.tendency[tend_idx], expected, 1e-15);
  }

  // Non-injection layers should be zero
  for (int ic = 0; ic < n_cells; ++ic) {
    size_t tend_idx = 0 * n_vert_levels * n_cells + 1 * n_cells + ic;
    EXPECT_DOUBLE_EQ(state.tendency[tend_idx], 0.0);
  }
}

TEST(FluxConverter, ElevatedInjection) {
  const int n_species = 1;
  const int n_cells = 2;
  const int n_vert_levels = 3;

  EmisState state(n_species, n_cells, n_vert_levels);
  state.surface_flux = {1e-6, 2e-6};
  state.injection_layer = {1};  // Inject at layer 1

  std::vector<Real> rho(n_vert_levels * n_cells, 1.0);
  std::vector<Real> dz(n_vert_levels * n_cells, 100.0);

  FluxConverter::Convert(state, rho.data(), dz.data(),
                         static_cast<int>(rho.size()));

  // Layer 0 should be zero
  EXPECT_DOUBLE_EQ(state.tendency[0 * n_cells + 0], 0.0);
  EXPECT_DOUBLE_EQ(state.tendency[0 * n_cells + 1], 0.0);

  // Layer 1 should have tendency
  EXPECT_NEAR(state.tendency[1 * n_cells + 0], 1e-6 / (1.0 * 100.0), 1e-15);
  EXPECT_NEAR(state.tendency[1 * n_cells + 1], 2e-6 / (1.0 * 100.0), 1e-15);

  // Layer 2 should be zero
  EXPECT_DOUBLE_EQ(state.tendency[2 * n_cells + 0], 0.0);
  EXPECT_DOUBLE_EQ(state.tendency[2 * n_cells + 1], 0.0);
}
