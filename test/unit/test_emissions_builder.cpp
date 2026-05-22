// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// EmissionsBuilder::Build() validation tests.  The builder folds all v1
// invariant checks into Build() (MICM-style: domain objects carry no
// Validate()).  Each invariant (V1-V5 from the plan plus the cross-source
// DuplicateCategoryHierarchy rule) is exercised in isolation.
//
// Build() is file-free for these cases: a rejected config throws before any
// source is constructed, and OfflineEmissionSource defers all NetCDF I/O to
// Run(), so an accepted config builds without touching disk.

#include <miem/source_types.hpp>
#include <miem/emissions.hpp>
#include <miem/emissions_builder.hpp>
#include <miem/species_map.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace miem;

// Assert that `stmt` throws a miem::MiemException carrying (cat, code).
#define EXPECT_MIEM_THROW(stmt, cat, code)                          \
  do {                                                              \
    try {                                                           \
      stmt;                                                         \
      ADD_FAILURE() << "expected miem::MiemException, none thrown"; \
    } catch (const ::miem::MiemException& e) {                      \
      EXPECT_STREQ(e.Category(), (cat));                            \
      EXPECT_EQ(e.Code(), (code));                                  \
    }                                                               \
  } while (0)

namespace {

// A minimal-but-valid source: one offline ECCAD source, no species
// mappings, default category/hierarchy.  Build() never opens the file.
Source MakeMinimalSource()
{
  Source src;
  src.name_         = "test";
  src.file_pattern_ = "/tmp/does_not_exist.nc";
  return src;
}

// Builder seeded with grid dimensions and a single minimal source.
EmissionsBuilder MinimalBuilder()
{
  EmissionsBuilder builder;
  builder.SetGridDimensions(/*n_cells=*/4, /*n_vert_levels=*/2)
         .AddSource(MakeMinimalSource());
  return builder;
}

}  // namespace

// ---------------------------------------------------------------------
// V1: regridding type must be None
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V1_RejectsRegriddingScrip)
{
  Regridding regridding;
  regridding.type_ = RegriddingType::Scrip;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2)
         .SetRegridding(regridding)
         .AddSource(MakeMinimalSource());

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_REGRIDDING);
}

// ---------------------------------------------------------------------
// V2: convention must be ECCAD (case-insensitive)
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V2_RejectsUnknownConvention)
{
  Source src = MakeMinimalSource();
  src.convention_ = "descriptor";  // not "eccad"

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION);
}

TEST(EmissionsBuilderValidateTest, V2_AcceptsCaseInsensitiveEccad)
{
  Source src = MakeMinimalSource();
  src.convention_ = "ECCAD";

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_NO_THROW(builder.Build());
}

// ---------------------------------------------------------------------
// V3: mode must be Offline
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V3_RejectsOnlineMode)
{
  Source src = MakeMinimalSource();
  src.mode_ = SourceMode::Online;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED);
}

// ---------------------------------------------------------------------
// V4: vertical injection must be Surface
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V4_RejectsPlumeInjection)
{
  Source src = MakeMinimalSource();
  src.vertical_injection_ = VerticalInjection::Plume;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_VERTICAL_INJECTION);
}

// ---------------------------------------------------------------------
// V5: scaling-factor sum > 1.0 + 1e-6 rejected
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V5_RejectsScalingSumOverOne)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.2);  // sum = 1.1

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_SPECIES,
                    MIEM_SPECIES_ERROR_CODE_SCALING_EXCEEDS_ONE);
}

// Boundary test: 1.0 + 1e-7 is within tolerance (1e-6) — accepted.
TEST(EmissionsBuilderValidateTest, V5_BoundaryAcceptsTinyOverhead)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-7));

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_NO_THROW(builder.Build());
}

// Boundary test: 1.0 + 1e-5 exceeds tolerance — rejected.
TEST(EmissionsBuilderValidateTest, V5_BoundaryRejectsClearOverhead)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-5));

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_SPECIES,
                    MIEM_SPECIES_ERROR_CODE_SCALING_EXCEEDS_ONE);
}

// ---------------------------------------------------------------------
// Duplicate (category, hierarchy) across two sources is rejected.
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsDuplicateCategoryHierarchy)
{
  Source a = MakeMinimalSource();
  a.name_ = "a";
  Source b = MakeMinimalSource();
  b.name_ = "b";  // identical (category=0, hierarchy=1)

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(a).AddSource(b);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_DUPLICATE_CATEGORY_HIERARCHY);
}

// ---------------------------------------------------------------------
// V6: a minimal source builds successfully (positive case).
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, V6_AcceptsMinimalConfig)
{
  EXPECT_NO_THROW(MinimalBuilder().Build());
}

// Throw-on-first: when a source fails several invariants at once (mode !=
// Offline AND vertical != Surface), Build() throws on the first one it
// checks — the V3 mode check (ONLINE_NOT_SUPPORTED) precedes the V4
// vertical-injection check.  (MICM-aligned exceptions abort on first
// error; multi-error accumulation was intentionally dropped.)
TEST(EmissionsBuilderValidateTest, ThrowsOnFirstInvariantOfMultiple)
{
  Source src = MakeMinimalSource();
  src.mode_               = SourceMode::Online;
  src.vertical_injection_ = VerticalInjection::Plume;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_MIEM_THROW(builder.Build(),
                    MIEM_ERROR_CATEGORY_CONFIGURATION,
                    MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED);
}

// When one source in a multi-source build is invalid, the error message
// names the offending source so the caller can locate it.
TEST(EmissionsBuilderValidateTest, ErrorMessageIdentifiesOffendingSource)
{
  Source good = MakeMinimalSource();
  good.name_     = "good_one";
  good.category_ = 0;

  Source bad = MakeMinimalSource();
  bad.name_      = "bad_one";
  bad.mode_      = SourceMode::Online;  // fails V3
  bad.category_  = 1;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(good).AddSource(bad);

  try
  {
    builder.Build();
    ADD_FAILURE() << "expected miem::MiemException, none thrown";
  }
  catch (const MiemException& e)
  {
    EXPECT_NE(std::string(e.what()).find("bad_one"), std::string::npos)
        << "expected error message to identify source 'bad_one'";
  }
}
