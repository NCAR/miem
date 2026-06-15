// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// EmissionsBuilder::Build() validation tests.  The builder folds every v1
// invariant check into Build() (MICM-style: domain objects carry no
// Validate()).  Each invariant -- the five per-source rules plus the
// cross-source duplicate (category, hierarchy) rule -- is exercised in
// isolation.
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

// Assert that `action` throws a miem::MiemException carrying (cat, code).
// gtest's EXPECT_THROW can't inspect the thrown object, so this helper
// runs the gtest assertions on the caught exception's category and code.
template <typename Action, typename Code>
void ExpectMiemThrow(Action&& action, const char* cat, Code code)
{
  try
  {
    action();
    ADD_FAILURE() << "expected miem::MiemException, none thrown";
  }
  catch (const ::miem::MiemException& e)
  {
    EXPECT_STREQ(e.Category(), cat);
    EXPECT_EQ(e.Code(), code);
  }
}

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
// Regridding type must be None
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsRegriddingScrip)
{
  Regridding regridding;
  regridding.type_ = RegriddingType::Scrip;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2)
         .SetRegridding(regridding)
         .AddSource(MakeMinimalSource());

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_CONFIGURATION,
                  MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_REGRIDDING);
}

// ---------------------------------------------------------------------
// Convention must be ECCAD (case-insensitive)
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsUnknownConvention)
{
  Source src = MakeMinimalSource();
  src.convention_ = "descriptor";  // not "eccad"

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_CONFIGURATION,
                  MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION);
}

TEST(EmissionsBuilderValidateTest, AcceptsCaseInsensitiveEccad)
{
  Source src = MakeMinimalSource();
  src.convention_ = "ECCAD";

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_NO_THROW(builder.Build());
}

// ---------------------------------------------------------------------
// Mode must be Offline
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsOnlineMode)
{
  Source src = MakeMinimalSource();
  src.mode_ = SourceMode::Online;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_CONFIGURATION,
                  MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED);
}

// ---------------------------------------------------------------------
// Vertical injection must be Surface
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsPlumeInjection)
{
  Source src = MakeMinimalSource();
  src.vertical_injection_ = VerticalInjection::Plume;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_CONFIGURATION,
                  MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_VERTICAL_INJECTION);
}

// ---------------------------------------------------------------------
// Scaling-factor sum > 1.0 + 1e-6 rejected
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, RejectsScalingSumOverOne)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",  0.9);
  src.species_map_.AddMapping("NOx", "NO2", 0.2);  // sum = 1.1

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_SPECIES,
                  MIEM_SPECIES_ERROR_CODE_SCALING_EXCEEDS_ONE);
}

// Boundary test: 1.0 + 1e-7 is within tolerance (1e-6) — accepted.
TEST(EmissionsBuilderValidateTest, BoundaryAcceptsTinyOverhead)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-7));

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  EXPECT_NO_THROW(builder.Build());
}

// Boundary test: 1.0 + 1e-5 exceeds tolerance — rejected.
TEST(EmissionsBuilderValidateTest, BoundaryRejectsClearOverhead)
{
  Source src = MakeMinimalSource();
  src.species_map_.AddMapping("NOx", "NO",
      static_cast<Real>(1.0) + static_cast<Real>(1e-5));

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
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

  ExpectMiemThrow([&] { builder.Build(); },
                  MIEM_ERROR_CATEGORY_CONFIGURATION,
                  MIEM_CONFIGURATION_ERROR_CODE_DUPLICATE_CATEGORY_HIERARCHY);
}

// ---------------------------------------------------------------------
// A minimal source builds successfully (positive case).
// ---------------------------------------------------------------------
TEST(EmissionsBuilderValidateTest, AcceptsMinimalConfig)
{
  EXPECT_NO_THROW(MinimalBuilder().Build());
}

// Throw-on-first: when a source fails several invariants at once (mode !=
// Offline AND vertical != Surface), Build() throws on the first one it
// checks — the mode check (ONLINE_NOT_SUPPORTED) precedes the
// vertical-injection check.  (MICM-aligned exceptions abort on first
// error; multi-error accumulation was intentionally dropped.)
TEST(EmissionsBuilderValidateTest, ThrowsOnFirstInvariantOfMultiple)
{
  Source src = MakeMinimalSource();
  src.mode_               = SourceMode::Online;
  src.vertical_injection_ = VerticalInjection::Plume;

  EmissionsBuilder builder;
  builder.SetGridDimensions(4, 2).AddSource(src);

  ExpectMiemThrow([&] { builder.Build(); },
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
  bad.mode_      = SourceMode::Online;  // fails the mode invariant
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
