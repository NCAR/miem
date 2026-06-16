// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/temporal_interpolator.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <algorithm>
#include <string>

namespace miem
{

  TemporalInterpolator::TemporalInterpolator(InterpolationMode mode)
      : mode_(mode)
  {
  }

  void TemporalInterpolator::SetBracket(
      double time_left,
      double time_right,
      const std::vector<Real>& data_left,
      const std::vector<Real>& data_right)
  {
    if (data_left.size() != data_right.size())
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_VALIDATION,
          MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH,
          "TemporalInterpolator::SetBracket: data_left size " + std::to_string(data_left.size()) + " != data_right size " +
              std::to_string(data_right.size()));
    }
    time_left_ = time_left;
    time_right_ = time_right;
    data_left_ = data_left;
    data_right_ = data_right;
  }

  bool TemporalInterpolator::CoversTime(double time) const
  {
    return time >= time_left_ && time <= time_right_;
  }

  void TemporalInterpolator::Interpolate(double time_current, std::vector<Real>& output) const
  {
    const std::size_t n = data_left_.size();
    output.resize(n);

    switch (mode_)
    {
      case InterpolationMode::kNone: output = data_left_; return;

      case InterpolationMode::kNearest:
      {
        const double mid = 0.5 * (time_left_ + time_right_);
        output = (time_current <= mid) ? data_left_ : data_right_;
        return;
      }

      case InterpolationMode::kLinear:
      {
        const double dt = time_right_ - time_left_;
        if (dt <= 0.0)
        {
          output = data_left_;
          return;
        }
        double alpha = (time_current - time_left_) / dt;
        alpha = std::max(0.0, std::min(1.0, alpha));
        for (std::size_t i = 0; i < n; ++i)
        {
          output[i] = static_cast<Real>(data_left_[i] * (1.0 - alpha) + data_right_[i] * alpha);
        }
        return;
      }
    }
  }

}  // namespace miem
