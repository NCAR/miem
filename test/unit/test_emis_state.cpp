#include <gtest/gtest.h>

#include "miem/emis_state.hpp"

using namespace miem;

TEST(EmisState, DefaultConstruction) {
  EmisState state;
  EXPECT_EQ(state.n_species, 0);
  EXPECT_EQ(state.n_cells, 0);
  EXPECT_EQ(state.n_vert_levels, 0);
  EXPECT_TRUE(state.surface_flux.empty());
  EXPECT_TRUE(state.tendency.empty());
}

TEST(EmisState, ParameterizedConstruction) {
  EmisState state(3, 100, 10);
  EXPECT_EQ(state.n_species, 3);
  EXPECT_EQ(state.n_cells, 100);
  EXPECT_EQ(state.n_vert_levels, 10);
  EXPECT_EQ(state.surface_flux.size(), 300u);  // 3 * 100
  EXPECT_EQ(state.tendency.size(), 3000u);      // 3 * 10 * 100
  EXPECT_EQ(state.emis_to_chem_idx.size(), 3u);
  EXPECT_EQ(state.injection_layer.size(), 3u);
}

TEST(EmisState, AllocationZeroInit) {
  EmisState state(2, 50, 5);
  for (const auto& v : state.surface_flux) {
    EXPECT_DOUBLE_EQ(v, 0.0);
  }
  for (const auto& v : state.tendency) {
    EXPECT_DOUBLE_EQ(v, 0.0);
  }
}

TEST(EmisState, Resize) {
  EmisState state;
  state.Resize(2, 10, 5);
  EXPECT_EQ(state.surface_flux.size(), 20u);
  EXPECT_EQ(state.tendency.size(), 100u);

  state.Resize(4, 20, 10);
  EXPECT_EQ(state.surface_flux.size(), 80u);
  EXPECT_EQ(state.tendency.size(), 800u);
}

TEST(EmisState, Zero) {
  EmisState state(2, 10, 5);
  state.surface_flux[0] = 1.0;
  state.tendency[0] = 2.0;

  state.Zero();
  EXPECT_DOUBLE_EQ(state.surface_flux[0], 0.0);
  EXPECT_DOUBLE_EQ(state.tendency[0], 0.0);
}

TEST(EmisState, DataPointers) {
  EmisState state(2, 10, 5);
  state.surface_flux[0] = 42.0;
  EXPECT_DOUBLE_EQ(state.SurfaceFluxData()[0], 42.0);
  EXPECT_EQ(state.SurfaceFluxData(), state.surface_flux.data());
  EXPECT_EQ(state.TendencyData(), state.tendency.data());
  EXPECT_EQ(state.EmisToChemIdxData(), state.emis_to_chem_idx.data());
}

TEST(EmisState, DefaultHostIndices) {
  EmisState state(3, 10, 5);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(state.emis_to_chem_idx[i], -1);
    EXPECT_EQ(state.injection_layer[i], 0);
  }
}
