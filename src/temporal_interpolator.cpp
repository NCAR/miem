#include "miem/temporal_interpolator.hpp"

#include <cmath>

#include "miem/util/error.hpp"

namespace miem {

InterpolationMode ParseInterpolationMode(const std::string& mode_str) {
  if (mode_str == "linear") return InterpolationMode::kLinear;
  if (mode_str == "nearest") return InterpolationMode::kNearest;
  if (mode_str == "none") return InterpolationMode::kNone;
  throw ConfigError("Unknown interpolation mode: '" + mode_str +
                    "'. Valid options: linear, nearest, none");
}

TemporalInterpolator::TemporalInterpolator(InterpolationMode mode)
    : mode_(mode) {}

void TemporalInterpolator::SetBracket(double time_left, double time_right,
                                      const std::vector<Real>& data_left,
                                      const std::vector<Real>& data_right) {
  if (data_left.size() != data_right.size()) {
    throw ValidationError(
        "TemporalInterpolator: bracket data arrays must have same size");
  }
  time_left_ = time_left;
  time_right_ = time_right;
  data_left_ = data_left;
  data_right_ = data_right;
}

bool TemporalInterpolator::CoversTime(double time) const {
  return time >= time_left_ && time <= time_right_;
}

void TemporalInterpolator::Interpolate(double time_current,
                                       std::vector<Real>& output) const {
  const size_t n = data_left_.size();
  output.resize(n);

  switch (mode_) {
    case InterpolationMode::kNone:
      // Use left bracket data (constant)
      output = data_left_;
      break;

    case InterpolationMode::kNearest: {
      double mid = 0.5 * (time_left_ + time_right_);
      const auto& src = (time_current <= mid) ? data_left_ : data_right_;
      output = src;
      break;
    }

    case InterpolationMode::kLinear: {
      double dt = time_right_ - time_left_;
      if (dt <= 0.0) {
        output = data_left_;
        return;
      }
      double alpha = (time_current - time_left_) / dt;
      // Clamp to [0, 1]
      alpha = std::max(0.0, std::min(1.0, alpha));

      for (size_t i = 0; i < n; ++i) {
        output[i] = data_left_[i] * (1.0 - alpha) + data_right_[i] * alpha;
      }
      break;
    }
  }
}

}  // namespace miem
