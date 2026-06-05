// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for `miem::Result<T>`.  Covers the basic Ok/Error API plus
// the H1 regression: `value()` on an empty Result must abort rather than
// silently dereference an empty std::optional (UB on the legacy path).

#include <miem/util/result.hpp>

#include <gtest/gtest.h>

#include <string>

using miem::ErrorCode;
using miem::Result;

TEST(ResultTest, OkValueIsTruthyAndHoldsValue)
{
  auto r = Result<int>::Ok(42);
  EXPECT_TRUE(static_cast<bool>(r));
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r.value(), 42);
  EXPECT_TRUE(r.errors().empty());
}

TEST(ResultTest, ErrorIsFalsyAndCarriesCode)
{
  auto r = Result<int>::Error(ErrorCode::ConfigInvalid, "bad config");
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_FALSE(r.has_value());
  ASSERT_EQ(r.errors().size(), 1u);
  EXPECT_EQ(r.errors().front().code_, ErrorCode::ConfigInvalid);
  EXPECT_EQ(r.errors().front().message_, "bad config");
}

TEST(ResultTest, VoidOkIsTruthy)
{
  auto r = Result<void>::Ok();
  EXPECT_TRUE(static_cast<bool>(r));
  EXPECT_TRUE(r.errors().empty());
}

TEST(ResultTest, VoidErrorCarriesCode)
{
  auto r = Result<void>::Error(ErrorCode::TimeOutOfRange, "out of range");
  EXPECT_FALSE(static_cast<bool>(r));
  ASSERT_EQ(r.errors().size(), 1u);
  EXPECT_EQ(r.errors().front().code_, ErrorCode::TimeOutOfRange);
}

TEST(ResultTest, ErrorsConstructorCarriesMultiple)
{
  std::vector<miem::ErrorEntry> errs = {
      { ErrorCode::ConfigInvalid, "first" },
      { ErrorCode::CellCountMismatch, "second" },
  };
  auto r = Result<int>::Errors(errs);
  EXPECT_FALSE(static_cast<bool>(r));
  ASSERT_EQ(r.errors().size(), 2u);
  EXPECT_EQ(r.errors()[0].code_, ErrorCode::ConfigInvalid);
  EXPECT_EQ(r.errors()[1].code_, ErrorCode::CellCountMismatch);
}

TEST(ResultTest, AddErrorAccumulates)
{
  Result<void> r;
  EXPECT_TRUE(static_cast<bool>(r));
  r.AddError(ErrorCode::ConfigInvalid, "first");
  EXPECT_FALSE(static_cast<bool>(r));
  r.AddError(ErrorCode::CellCountMismatch, "second");
  EXPECT_EQ(r.errors().size(), 2u);
}

TEST(ResultTest, MoveValueOutOfRvalue)
{
  auto r = Result<std::string>::Ok(std::string{ "hello" });
  ASSERT_TRUE(static_cast<bool>(r));
  std::string moved = std::move(r).value();
  EXPECT_EQ(moved, "hello");
}

// ---------------------------------------------------------------------
// H1 regression: `Result<T>::value()` on an empty Result must abort with
// the MIEM_ASSERT message rather than dereference an empty optional
// (legacy UB).  We test via an EXPECT_DEATH macro because the assert
// triggers std::abort.
//
// EXPECT_DEATH skipped on platforms without fork support; here we rely
// on macOS / Linux behaviour, which the project targets.
// ---------------------------------------------------------------------

TEST(ResultDeathTest, ValueOnEmptyAborts)
{
  // GoogleTest convention: tests using EXPECT_DEATH should be in a
  // *DeathTest fixture so they run before threaded tests would
  // otherwise destabilise the fork.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  auto r = Result<int>::Error(ErrorCode::ConfigInvalid, "no value");
  EXPECT_DEATH({ (void)r.value(); }, "MIEM_ASSERT failed");
}

TEST(ResultDeathTest, ValueOnDefaultConstructedAborts)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  Result<int> r;  // default-constructed: no value, no errors
  EXPECT_DEATH({ (void)r.value(); }, "MIEM_ASSERT failed");
}
