// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Unit tests for `TemporalInterpolator`.  Plan §"Temporal interpolation":
// linear at midpoint, endpoints, clamping, nearest left-bias, none,
// degenerate bracket, SetBracket size-mismatch error.

#include <miem/temporal_interpolator.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <gtest/gtest.h>

#include <vector>

using namespace miem;

namespace
{

  const std::vector<Real> kLeft = { Real{ 1.0 } };
  const std::vector<Real> kRight = { Real{ 3.0 } };

}  // namespace

// ---------------------------------------------------------------------
// Linear @ midpoint: (0, 100, [1.0], [3.0]) -> interpolate(50) = [2.0]
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, LinearAtMidpoint)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  ASSERT_NO_THROW(t.SetBracket(0.0, 100.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(50.0, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 2.0);
}

TEST(TemporalInterpolatorTest, LinearAtEndpoints)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  ASSERT_NO_THROW(t.SetBracket(0.0, 100.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(0.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);

  t.Interpolate(100.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 3.0);
}

// Linear-mode `Interpolate` is total and clamps the alpha to [0,1].  The
// caller (OfflineEmissionSource) is responsible for the actual out-of-
// range hard-error guard.
TEST(TemporalInterpolatorTest, LinearClampsOutsideBracket)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  ASSERT_NO_THROW(t.SetBracket(0.0, 100.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(-50.0, out);  // below left
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);

  t.Interpolate(200.0, out);  // beyond right
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 3.0);
}

// ---------------------------------------------------------------------
// Nearest mode: midpoint biased left (time <= mid -> left).
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, NearestModeLeftBiasAtMidpoint)
{
  TemporalInterpolator t(InterpolationMode::kNearest);
  ASSERT_NO_THROW(t.SetBracket(0.0, 100.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(50.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);  // left wins ties

  t.Interpolate(75.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 3.0);

  t.Interpolate(25.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);
}

// ---------------------------------------------------------------------
// None mode: any time returns left.
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, NoneModeReturnsLeft)
{
  TemporalInterpolator t(InterpolationMode::kNone);
  ASSERT_NO_THROW(t.SetBracket(0.0, 100.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(99.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);
  t.Interpolate(0.5, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);
}

// ---------------------------------------------------------------------
// Degenerate bracket (time_left == time_right) -> linear yields left.
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, DegenerateBracketReturnsLeft)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  ASSERT_NO_THROW(t.SetBracket(50.0, 50.0, kLeft, kRight));

  std::vector<Real> out;
  t.Interpolate(50.0, out);
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);
  t.Interpolate(60.0, out);  // anywhere — still left, no div-by-zero
  EXPECT_DOUBLE_EQ(static_cast<double>(out[0]), 1.0);
}

// ---------------------------------------------------------------------
// SetBracket with size mismatch -> throws Validation / CELL_COUNT_MISMATCH
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, SetBracketSizeMismatchThrows)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  std::vector<Real> left = { Real{ 1 }, Real{ 2 } };
  std::vector<Real> right = { Real{ 3 } };
  EXPECT_THROW(
      {
        try
        {
          t.SetBracket(0.0, 100.0, left, right);
        }
        catch (const miem::MiemException& e)
        {
          EXPECT_STREQ(e.category_, MIEM_ERROR_CATEGORY_VALIDATION);
          EXPECT_EQ(e.code_, MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH);
          throw;
        }
      },
      miem::MiemException);
}

// ---------------------------------------------------------------------
// CoversTime / TimeLeft / TimeRight accessors round-trip the bracket.
// ---------------------------------------------------------------------
TEST(TemporalInterpolatorTest, CoversTimeAccessor)
{
  TemporalInterpolator t(InterpolationMode::kLinear);
  ASSERT_NO_THROW(t.SetBracket(10.0, 20.0, kLeft, kRight));

  EXPECT_TRUE(t.CoversTime(10.0));
  EXPECT_TRUE(t.CoversTime(15.0));
  EXPECT_TRUE(t.CoversTime(20.0));
  EXPECT_FALSE(t.CoversTime(9.999));
  EXPECT_FALSE(t.CoversTime(20.001));
  EXPECT_DOUBLE_EQ(t.TimeLeft(), 10.0);
  EXPECT_DOUBLE_EQ(t.TimeRight(), 20.0);
}
