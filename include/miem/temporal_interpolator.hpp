// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Two-time-slice bracket interpolator: linear, nearest, or hold-left.
// `SetBracket` validates that the two slices have matching size and
// returns `Result<void>::Error{CellCountMismatch}` on failure.
// `Interpolate` is total (no throws, no errors) — out-of-range gating
// is the caller's responsibility (see OfflineEmissionSource).
#pragma once

#include <vector>

#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

enum class InterpolationMode
{
  kLinear,
  kNearest,
  kNone,
};

class TemporalInterpolator
{
 public:
  TemporalInterpolator() = default;
  explicit TemporalInterpolator(InterpolationMode mode);

  // Set the two-slice bracket.  Returns CellCountMismatch when the two
  // arrays have different size.
  Result<void> SetBracket(double                   time_left,
                          double                   time_right,
                          const std::vector<Real>& data_left,
                          const std::vector<Real>& data_right);

  // Interpolate to `time_current` into `output` (resized to bracket size).
  // Total function; no error path.  Linear-mode clamps the alpha to [0,1]
  // — the caller (OfflineEmissionSource) is responsible for keeping
  // `time_current` within the bracket's `[time_left, time_right]` range.
  void Interpolate(double time_current, std::vector<Real>& output) const;

  // True iff `time` lies inside the current bracket's time interval.
  bool CoversTime(double time) const;

  InterpolationMode Mode()      const { return mode_; }
  double            TimeLeft()  const { return time_left_; }
  double            TimeRight() const { return time_right_; }

 private:
  InterpolationMode mode_       = InterpolationMode::kLinear;
  double            time_left_  = 0.0;
  double            time_right_ = 0.0;
  std::vector<Real> data_left_;
  std::vector<Real> data_right_;
};

}  // namespace miem
