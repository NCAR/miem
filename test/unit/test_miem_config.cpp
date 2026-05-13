// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Tests for `MIEMConfig::Validate()` and the defaults audit.  Each
// validator branch (V1-V6 from the plan plus DuplicateCategoryHierarchy)
// is exercised in isolation; the defaults audit asserts that a minimal
// `SourceConfig` only carrying name + file_pattern validates and that
// each defaulted field has the documented value.

#include <miem/config.hpp>
#include <miem/species_map.hpp>
#include <miem/util/result.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

using namespace miem;

namespace {

// Build a minimal-but-valid config (one offline ECCAD source, no species
// mappings, default category/hierarchy).
MIEMConfig MakeMinimalConfig()
{
  SourceConfig src;
  src.name_         = "test";
  src.file_pattern_ = "/tmp/does_not_exist.nc";  // validate doesn't touch disk

  MIEMConfig cfg;
  cfg.version_ = "1.0.0";
  cfg.sources_ = { src };
  return cfg;
}

// Predicate helper: does the error list contain code `c`?
bool HasErrorCode(const Result<void>& r, ErrorCode c)
{
  for (const auto& e : r.errors())
  {
    if (e.code_ == c) return true;
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------
// V1: regridding type must be None
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V1_RejectsRegriddingScrip)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.regridding_.type_ = RegriddingType::Scrip;

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::UnsupportedRegriddingType));
}

// ---------------------------------------------------------------------
// V2: convention must be ECCAD (case-insensitive)
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V2_RejectsUnknownConvention)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].convention_ = "descriptor";  // not "eccad"

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::UnknownConvention));
}

TEST(MIEMConfigValidateTest, V2_AcceptsCaseInsensitiveEccad)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].convention_ = "ECCAD";

  auto r = cfg.Validate();
  EXPECT_TRUE(static_cast<bool>(r))
      << (r.errors().empty() ? "" : r.errors().front().message_);
}

// ---------------------------------------------------------------------
// V3: mode must be Offline
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V3_RejectsOnlineMode)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].mode_ = SourceMode::Online;

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::OnlineSourcesNotSupported));
}

// ---------------------------------------------------------------------
// V4: vertical injection must be Surface
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V4_RejectsPlumeInjection)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].vertical_injection_ = VerticalInjection::Plume;

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::UnsupportedVerticalInjection));
}

// ---------------------------------------------------------------------
// V5: scaling-factor sum > 1.0 + 1e-6 rejected
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V5_RejectsScalingSumOverOne)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].species_map_.AddMapping("NOx", "NO",  0.9);
  cfg.sources_[0].species_map_.AddMapping("NOx", "NO2", 0.2);  // sum = 1.1

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::SpeciesMapScalingExceedsOne));
}

// Boundary test: 1.0 + 1e-7 is within tolerance (1e-6) — accepted.
TEST(MIEMConfigValidateTest, V5_BoundaryAcceptsTinyOverhead)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-7));

  auto r = cfg.Validate();
  EXPECT_TRUE(static_cast<bool>(r))
      << (r.errors().empty() ? "" : r.errors().front().message_);
}

// Boundary test: 1.0 + 1e-5 exceeds tolerance — rejected.
TEST(MIEMConfigValidateTest, V5_BoundaryRejectsClearOverhead)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-5));

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::SpeciesMapScalingExceedsOne));
}

// ---------------------------------------------------------------------
// Duplicate (category, hierarchy) rejected
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, RejectsDuplicateCategoryHierarchy)
{
  MIEMConfig cfg = MakeMinimalConfig();
  // Add a second source with identical (category=0, hierarchy=1).
  SourceConfig dup = cfg.sources_[0];
  dup.name_ = "test2";
  cfg.sources_.push_back(dup);

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::DuplicateCategoryHierarchy));
}

// ---------------------------------------------------------------------
// V6: minimal valid config accepted (positive case)
// ---------------------------------------------------------------------
TEST(MIEMConfigValidateTest, V6_AcceptsMinimalConfig)
{
  MIEMConfig cfg = MakeMinimalConfig();
  auto r = cfg.Validate();
  EXPECT_TRUE(static_cast<bool>(r))
      << (r.errors().empty() ? "" : r.errors().front().message_);
}

// ---------------------------------------------------------------------
// Defaults audit: a bare SourceConfig with only name + file_pattern set
// must validate, and each defaulted field must have its documented value.
// ---------------------------------------------------------------------
TEST(MIEMConfigDefaultsTest, BareSourceDefaultsMatchHeader)
{
  SourceConfig src;
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

TEST(MIEMConfigDefaultsTest, BareMIEMConfigDefaultsMatchHeader)
{
  MIEMConfig cfg;
  EXPECT_TRUE(cfg.version_.empty());
  EXPECT_EQ(cfg.regridding_.type_, RegriddingType::None);
  EXPECT_TRUE(cfg.regridding_.weights_file_.empty());
  EXPECT_TRUE(cfg.sources_.empty());
}

TEST(MIEMConfigDefaultsTest, MinimalConfigPassesValidate)
{
  MIEMConfig cfg = MakeMinimalConfig();
  auto r = cfg.Validate();
  EXPECT_TRUE(static_cast<bool>(r));
}

// Multi-error accumulation: one source can fail several invariants at once
// (mode != Offline AND vertical != Surface) — both should be reported.
TEST(MIEMConfigValidateTest, AccumulatesMultipleErrorsOnOneSource)
{
  MIEMConfig cfg = MakeMinimalConfig();
  cfg.sources_[0].mode_               = SourceMode::Online;
  cfg.sources_[0].vertical_injection_ = VerticalInjection::Plume;

  auto r = cfg.Validate();
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::OnlineSourcesNotSupported));
  EXPECT_TRUE(HasErrorCode(r, ErrorCode::UnsupportedVerticalInjection));
}
