// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Defaults audit for the `Source` description type.  A bare `Source` with
// only name + file_pattern set must carry the documented defaults, and its
// embedded `SpeciesMap` must start empty.  The v1 invariant checks
// (eccad/offline/surface, scaling sum, duplicate category+hierarchy) are
// enforced by `EmissionsBuilder::Build()` and tested alongside the builder.

#include <miem/source_types.hpp>
#include <miem/species_map.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace miem;

// A bare Source with only name + file_pattern set must match the
// documented defaults field-for-field.
TEST(SourceDefaultsTest, BareSourceDefaultsMatchHeader)
{
  Source src;
  src.name_         = "defaults_audit";
  src.file_pattern_ = "/tmp/dummy.nc";

  EXPECT_EQ(src.mode_,                   SourceMode::Offline);
  EXPECT_EQ(src.type_,                   SourceType::Anthropogenic);
  EXPECT_EQ(src.convention_,             "eccad");
  EXPECT_EQ(src.temporal_interpolation_, TemporalInterpolation::Linear);
  EXPECT_EQ(src.vertical_injection_,     VerticalInjection::Surface);
  EXPECT_EQ(src.category_,               0);
  EXPECT_EQ(src.hierarchy_,              1);
  EXPECT_DOUBLE_EQ(static_cast<double>(src.scaling_factor_), 1.0);
  EXPECT_TRUE(src.sector_.empty());
  EXPECT_TRUE(src.species_map_.Mappings().empty());
}

// A default-constructed Regridding is None (the only v1-supported mode)
// with no weights file.
TEST(SourceDefaultsTest, BareRegriddingDefaultsMatchHeader)
{
  Regridding regridding;
  EXPECT_EQ(regridding.type_, RegriddingType::None);
  EXPECT_TRUE(regridding.weights_file_.empty());
}

// The embedded species map is a live SpeciesMap: mappings added to a
// Source are observable through it.
TEST(SourceDefaultsTest, SpeciesMapIsMutableInPlace)
{
  Source src;
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.1);

  EXPECT_EQ(src.species_map_.Mappings().size(), 2u);
}
