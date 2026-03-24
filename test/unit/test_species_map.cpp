#include <gtest/gtest.h>

#include <fstream>

#include "miem/species_map.hpp"

using namespace miem;

class SpeciesMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test YAML file
    std::ofstream f("test_species_map.yaml");
    f << R"(
species_map:
  mechanism: "MOZART-T1"
  mappings:
    - inventory: "NOx"
      mechanism: "NO"
      scaling: 0.9
    - inventory: "NOx"
      mechanism: "NO2"
      scaling: 0.1
    - inventory: "SO2"
      mechanism: "SO2"
      scaling: 1.0
    - inventory: "CO"
      mechanism: "CO"
)";
    f.close();
  }

  void TearDown() override {
    std::remove("test_species_map.yaml");
  }
};

TEST_F(SpeciesMapTest, LoadFromYAML) {
  SpeciesMap map("test_species_map.yaml");
  EXPECT_EQ(map.MechanismName(), "MOZART-T1");
  EXPECT_EQ(map.Mappings().size(), 4u);
}

TEST_F(SpeciesMapTest, MechanismSpecies) {
  SpeciesMap map("test_species_map.yaml");
  auto species = map.MechanismSpecies();
  EXPECT_EQ(species.size(), 4u);
  // Set returns sorted order
  EXPECT_NE(std::find(species.begin(), species.end(), "NO"), species.end());
  EXPECT_NE(std::find(species.begin(), species.end(), "NO2"), species.end());
  EXPECT_NE(std::find(species.begin(), species.end(), "SO2"), species.end());
  EXPECT_NE(std::find(species.begin(), species.end(), "CO"), species.end());
}

TEST_F(SpeciesMapTest, InventorySpecies) {
  SpeciesMap map("test_species_map.yaml");
  auto species = map.InventorySpecies();
  EXPECT_EQ(species.size(), 3u);  // NOx, SO2, CO (unique)
}

TEST_F(SpeciesMapTest, Apply1To1Mapping) {
  SpeciesMap map;
  map.AddMapping("SO2", "SO2", 1.0);

  int n_cells = 3;
  std::vector<Real> inv_flux = {1.0, 2.0, 3.0};
  std::vector<std::string> inv_names = {"SO2"};
  std::vector<Real> mech_flux;

  map.Apply(inv_flux, inv_names, mech_flux, n_cells);

  EXPECT_EQ(mech_flux.size(), 3u);
  EXPECT_DOUBLE_EQ(mech_flux[0], 1.0);
  EXPECT_DOUBLE_EQ(mech_flux[1], 2.0);
  EXPECT_DOUBLE_EQ(mech_flux[2], 3.0);
}

TEST_F(SpeciesMapTest, Apply1ToNFanOut) {
  SpeciesMap map;
  map.AddMapping("NOx", "NO", 0.9);
  map.AddMapping("NOx", "NO2", 0.1);

  int n_cells = 2;
  std::vector<Real> inv_flux = {10.0, 20.0};
  std::vector<std::string> inv_names = {"NOx"};
  std::vector<Real> mech_flux;

  map.Apply(inv_flux, inv_names, mech_flux, n_cells);

  auto mech_species = map.MechanismSpecies();  // {"NO", "NO2"} sorted
  int no_idx = -1, no2_idx = -1;
  for (int i = 0; i < static_cast<int>(mech_species.size()); ++i) {
    if (mech_species[i] == "NO") no_idx = i;
    if (mech_species[i] == "NO2") no2_idx = i;
  }

  EXPECT_NEAR(mech_flux[no_idx * n_cells + 0], 9.0, 1e-10);
  EXPECT_NEAR(mech_flux[no_idx * n_cells + 1], 18.0, 1e-10);
  EXPECT_NEAR(mech_flux[no2_idx * n_cells + 0], 1.0, 1e-10);
  EXPECT_NEAR(mech_flux[no2_idx * n_cells + 1], 2.0, 1e-10);
}

TEST_F(SpeciesMapTest, AddMappingProgrammatic) {
  SpeciesMap map;
  map.AddMapping("CO", "CO", 1.0);
  EXPECT_EQ(map.Mappings().size(), 1u);
  EXPECT_EQ(map.MechanismSpecies().size(), 1u);
}
