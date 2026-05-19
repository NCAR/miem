// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

namespace miem {

enum class SourceMode { Offline, Online };
enum class SourceType { Anthropogenic, Fire, Biogenic, Dust, SeaSalt, Lightning };
enum class TemporalInterpolation { Linear, Nearest };
enum class VerticalInjection { Surface, Plume };

struct SourceConfig
{
  std::string name_;
  SourceMode mode_                           = SourceMode::Offline;
  SourceType type_                           = SourceType::Anthropogenic;
  std::string file_pattern_;
  std::string provider_;
  TemporalInterpolation temporal_interpolation_ = TemporalInterpolation::Linear;
  VerticalInjection vertical_injection_      = VerticalInjection::Surface;
  // Category/hierarchy follows the HEMCO aggregation pattern:
  // Sources in different categories are summed together.
  // Within the same category, the source with the highest hierarchy
  // value wins per cell, allowing regional inventories to override
  // global ones (e.g., EPA data over CEDS for US cells).
  int category_                              = 0;
  int hierarchy_                             = 1;
  double scaling_factor_                     = 1.0;
  std::string sector_;
};

struct MIEMConfig
{
  std::vector<SourceConfig> sources_;
};

}  // namespace miem
