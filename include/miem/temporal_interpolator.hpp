#pragma once

#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

enum class InterpolationMode {
  kLinear,
  kNearest,
  kNone
};

InterpolationMode ParseInterpolationMode(const std::string& mode_str);

class TemporalInterpolator {
 public:
  TemporalInterpolator() = default;
  explicit TemporalInterpolator(InterpolationMode mode);

  // Set the bracket data for interpolation.
  // time_left/time_right: timestamps of the bracketing time slices
  // data_left/data_right: flux arrays at those times (n_species * n_cells)
  void SetBracket(double time_left, double time_right,
                  const std::vector<Real>& data_left,
                  const std::vector<Real>& data_right);

  // Interpolate to a target time, writing result into output.
  void Interpolate(double time_current, std::vector<Real>& output) const;

  // Check if the current bracket covers the given time
  bool CoversTime(double time) const;

  InterpolationMode Mode() const { return mode_; }

  double TimeLeft() const { return time_left_; }
  double TimeRight() const { return time_right_; }

 private:
  InterpolationMode mode_ = InterpolationMode::kLinear;
  double time_left_ = 0.0;
  double time_right_ = 0.0;
  std::vector<Real> data_left_;
  std::vector<Real> data_right_;
};

}  // namespace miem
