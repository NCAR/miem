#include <gtest/gtest.h>

#include "miem/emissions_module.hpp"

using namespace miem;

// Full emissions module tests require NetCDF test data + YAML config.
// These tests verify the interface and error handling.

TEST(EmissionsModule, QuerySpeciesMissingConfigThrows) {
  EXPECT_THROW(EmissionsModule::QuerySpecies("nonexistent.yaml"), ConfigError);
}

TEST(EmissionsModule, ConstructMissingConfigThrows) {
  EXPECT_THROW(EmissionsModule("nonexistent.yaml", 100, 10), ConfigError);
}
