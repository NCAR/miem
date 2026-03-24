#include <gtest/gtest.h>

#include "miem/temporal_interpolator.hpp"
#include "miem/util/error.hpp"

using namespace miem;

TEST(TemporalInterpolator, ParseModes) {
  EXPECT_EQ(ParseInterpolationMode("linear"), InterpolationMode::kLinear);
  EXPECT_EQ(ParseInterpolationMode("nearest"), InterpolationMode::kNearest);
  EXPECT_EQ(ParseInterpolationMode("none"), InterpolationMode::kNone);
  EXPECT_THROW(ParseInterpolationMode("invalid"), ConfigError);
}

TEST(TemporalInterpolator, LinearMidpoint) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {0.0, 10.0};
  std::vector<Real> right = {2.0, 20.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.5, output);

  EXPECT_NEAR(output[0], 1.0, 1e-10);
  EXPECT_NEAR(output[1], 15.0, 1e-10);
}

TEST(TemporalInterpolator, LinearLeftEdge) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {1.0};
  std::vector<Real> right = {3.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.0, output);

  EXPECT_NEAR(output[0], 1.0, 1e-10);
}

TEST(TemporalInterpolator, LinearRightEdge) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {1.0};
  std::vector<Real> right = {3.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(1.0, output);

  EXPECT_NEAR(output[0], 3.0, 1e-10);
}

TEST(TemporalInterpolator, LinearQuarterPoint) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {0.0};
  std::vector<Real> right = {4.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.25, output);

  EXPECT_NEAR(output[0], 1.0, 1e-10);
}

TEST(TemporalInterpolator, NearestLeft) {
  TemporalInterpolator interp(InterpolationMode::kNearest);
  std::vector<Real> left = {1.0};
  std::vector<Real> right = {3.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.3, output);

  EXPECT_NEAR(output[0], 1.0, 1e-10);
}

TEST(TemporalInterpolator, NearestRight) {
  TemporalInterpolator interp(InterpolationMode::kNearest);
  std::vector<Real> left = {1.0};
  std::vector<Real> right = {3.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.7, output);

  EXPECT_NEAR(output[0], 3.0, 1e-10);
}

TEST(TemporalInterpolator, NoneMode) {
  TemporalInterpolator interp(InterpolationMode::kNone);
  std::vector<Real> left = {42.0};
  std::vector<Real> right = {99.0};
  interp.SetBracket(0.0, 1.0, left, right);

  std::vector<Real> output;
  interp.Interpolate(0.5, output);

  EXPECT_NEAR(output[0], 42.0, 1e-10);
}

TEST(TemporalInterpolator, CoversTime) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {1.0};
  std::vector<Real> right = {2.0};
  interp.SetBracket(10.0, 20.0, left, right);

  EXPECT_TRUE(interp.CoversTime(10.0));
  EXPECT_TRUE(interp.CoversTime(15.0));
  EXPECT_TRUE(interp.CoversTime(20.0));
  EXPECT_FALSE(interp.CoversTime(9.99));
  EXPECT_FALSE(interp.CoversTime(20.01));
}

TEST(TemporalInterpolator, MismatchedBracketSizeThrows) {
  TemporalInterpolator interp(InterpolationMode::kLinear);
  std::vector<Real> left = {1.0, 2.0};
  std::vector<Real> right = {3.0};
  EXPECT_THROW(interp.SetBracket(0.0, 1.0, left, right), ValidationError);
}
