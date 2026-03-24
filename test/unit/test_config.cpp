#include <gtest/gtest.h>

#include <fstream>

#include "miem/config.hpp"
#include "miem/dataset_descriptor.hpp"

using namespace miem;

class ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test MIEM config
    std::ofstream f("test_miem_config.yaml");
    f << R"(
miem:
  version: "1.0"
  sources:
    - name: "test_anthro"
      type: "anthropogenic"
      file_pattern: "test_data/anthro.nc"
      species_map: "test_species_map.yaml"
      temporal_interpolation: "linear"
      vertical_injection: "surface"
    - name: "test_fire"
      type: "fire"
      file_pattern: "test_data/fire.nc"
      species_map: "test_species_map.yaml"
      temporal_interpolation: "nearest"
      descriptor: "test_descriptor.yaml"
)";
    f.close();

    // Create test descriptor
    std::ofstream d("test_descriptor.yaml");
    d << R"(
descriptor:
  variable_prefix: "fire_"
  flux_units: "molecules/cm2/s"
  unit_conversion_factor: 1.66054e-21
  time_dimension: "time"
  cell_dimension: "ncol"
  species_rename:
    fire_NOx: "emi_NOx"
)";
    d.close();
  }

  void TearDown() override {
    std::remove("test_miem_config.yaml");
    std::remove("test_descriptor.yaml");
  }
};

TEST_F(ConfigTest, LoadConfig) {
  auto config = MIEMConfig::FromYAML("test_miem_config.yaml");
  EXPECT_EQ(config.version, "1.0");
  EXPECT_EQ(config.sources.size(), 2u);
}

TEST_F(ConfigTest, SourceConfig) {
  auto config = MIEMConfig::FromYAML("test_miem_config.yaml");

  const auto& anthro = config.sources[0];
  EXPECT_EQ(anthro.name, "test_anthro");
  EXPECT_EQ(anthro.type, "anthropogenic");
  EXPECT_EQ(anthro.temporal_interpolation, InterpolationMode::kLinear);
  EXPECT_TRUE(anthro.descriptor_path.empty());

  const auto& fire = config.sources[1];
  EXPECT_EQ(fire.name, "test_fire");
  EXPECT_EQ(fire.temporal_interpolation, InterpolationMode::kNearest);
  EXPECT_EQ(fire.descriptor_path, "test_descriptor.yaml");
}

TEST_F(ConfigTest, LoadDescriptor) {
  auto desc = DatasetDescriptor::FromYAML("test_descriptor.yaml");
  EXPECT_EQ(desc.variable_prefix, "fire_");
  EXPECT_NEAR(desc.unit_conversion_factor, 1.66054e-21, 1e-30);
  EXPECT_EQ(desc.time_dimension, "time");
  EXPECT_EQ(desc.cell_dimension, "ncol");
  EXPECT_EQ(desc.species_rename.size(), 1u);
  EXPECT_EQ(desc.species_rename.at("fire_NOx"), "emi_NOx");
}

TEST_F(ConfigTest, DefaultDescriptor) {
  auto desc = DatasetDescriptor::Default();
  EXPECT_EQ(desc.variable_prefix, "emi_");
  EXPECT_DOUBLE_EQ(desc.unit_conversion_factor, 1.0);
}

TEST_F(ConfigTest, MissingFileThrows) {
  EXPECT_THROW(MIEMConfig::FromYAML("nonexistent.yaml"), ConfigError);
}

TEST_F(ConfigTest, MissingMIEMKeyThrows) {
  std::ofstream f("bad_config.yaml");
  f << "version: 1.0\n";
  f.close();
  EXPECT_THROW(MIEMConfig::FromYAML("bad_config.yaml"), ConfigError);
  std::remove("bad_config.yaml");
}
