// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Public POD config types.  Trailing-underscore field convention is
// deliberate; see plan §D2 ("Preserve PR #8's trailing-underscore
// convention on public POD structs").
#pragma once

#include <string>
#include <vector>

#include "miem/species_map.hpp"
#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

enum class SourceMode             { Offline, Online };
enum class SourceType             { Anthropogenic, Fire, Biogenic, Dust, SeaSalt, Lightning };
enum class TemporalInterpolation  { Linear, Nearest, None };
enum class VerticalInjection      { Surface, Plume };
enum class RegriddingType         { None, Scrip };

// Top-level regridding spec.  v1 accepts only `type_ == None`.
struct Regridding
{
  RegriddingType type_ = RegriddingType::None;
  std::string    weights_file_;  // unused while type_ == None
};

// Configuration for a single emission source.
//
// Category/hierarchy follows the HEMCO pattern:
//   * Sources in different categories are summed.
//   * Within a category the highest-hierarchy source wins per cell.
// Duplicate (category, hierarchy) pairs are a load-time error.
struct SourceConfig
{
  std::string             name_;
  SourceMode              mode_                    = SourceMode::Offline;
  SourceType              type_                    = SourceType::Anthropogenic;

  // File pattern with optional {YYYY}{MM}{DD}{HH} tokens.  Already
  // resolved by musica's translator: MIEM receives a concrete path.
  std::string             file_pattern_;

  // Inventory convention.  v1 accepts only "eccad" (case-insensitive).
  // Anything else is `UnknownConvention` at validate time.
  std::string             convention_              = "eccad";

  // Programmatic species map (musica builds this by translating the parsed
  // YAML's named species_map entry).
  SpeciesMap              species_map_;

  TemporalInterpolation   temporal_interpolation_  = TemporalInterpolation::Linear;
  VerticalInjection       vertical_injection_      = VerticalInjection::Surface;
  int                     category_                = 0;
  int                     hierarchy_               = 1;
  Real                    scaling_factor_          = 1.0;
  std::string             sector_;  // optional diagnostic label
};

// Top-level MIEM configuration.
struct EmissionsConfig
{
  Regridding                regridding_;
  std::vector<SourceConfig> sources_;

  // Validate the config.  Returns `Result<void>::Ok()` if every check
  // passes, or a populated `errors()` vector otherwise.  Plan §"Resolved
  // decisions" D6, D7 — invariants:
  //   V1: regridding_.type_ == RegriddingType::None
  //   V2: each source convention_ == "eccad" (case-insensitive)
  //   V3: each source mode_       == SourceMode::Offline
  //   V4: each source vertical_injection_ == VerticalInjection::Surface
  //   V5: each source species_map_.Validate() succeeds (scaling ≤ 1.0)
  //   --: duplicate (category_, hierarchy_) is rejected
  Result<void> Validate() const;
};

}  // namespace miem
