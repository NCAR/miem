// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <miem/util/types.hpp>

#include <vector>

namespace miem
{

  /// @brief Interpolation mode for the two-slice bracket.
  enum class InterpolationMode
  {
    kLinear,   ///< Linear blend between the two slices.
    kNearest,  ///< Snap to the nearer slice in time.
    kNone,     ///< Hold the left slice (no blending).
  };

  /// @brief Two-time-slice bracket interpolator (linear, nearest, or
  ///        hold-left).
  ///
  /// @ref SetBracket validates that the two slices have matching size and
  /// throws on failure. @ref Interpolate is total (no throws, no errors);
  /// out-of-range gating is the caller's responsibility (see
  /// OfflineEmissionSource).
  class TemporalInterpolator
  {
   public:
    TemporalInterpolator() = default;
    explicit TemporalInterpolator(InterpolationMode mode);

    /// @brief Set the two-slice bracket.
    ///
    /// @throws MiemException (Validation / CELL_COUNT_MISMATCH) when the two
    ///         arrays have different size.
    void
    SetBracket(double time_left, double time_right, const std::vector<Real>& data_left, const std::vector<Real>& data_right);

    /// @brief Interpolate to @p time_current into @p output (resized to the
    ///        bracket size).
    ///
    /// Total function; no error path. Linear mode clamps the blend factor to
    /// the range [0, 1]; the caller (OfflineEmissionSource) is responsible
    /// for keeping @p time_current within the bracket's
    /// [@c time_left, @c time_right] range.
    void Interpolate(double time_current, std::vector<Real>& output) const;

    /// @brief True iff @p time lies inside the current bracket's interval.
    bool CoversTime(double time) const;

    /// @brief True once a bracket has been set via @ref SetBracket. A set
    ///        bracket always holds slice data, so this derives from it
    ///        rather than tracking a separate flag.
    bool HasBracket() const
    {
      return !data_left_.empty();
    }

    /// @brief The active interpolation mode.
    InterpolationMode Mode() const
    {
      return mode_;
    }
    /// @brief Left edge of the current bracket [s].
    double TimeLeft() const
    {
      return time_left_;
    }
    /// @brief Right edge of the current bracket [s].
    double TimeRight() const
    {
      return time_right_;
    }

   private:
    InterpolationMode mode_ = InterpolationMode::kLinear;
    double time_left_ = 0.0;
    double time_right_ = 0.0;
    std::vector<Real> data_left_;
    std::vector<Real> data_right_;
  };

}  // namespace miem
