// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Public POD description types for an emission source.  MIEM mirrors
// MICM: there is no aggregate "config" object — a host (or musica's
// translator) hands the runtime a collection of domain objects.  `Source`
// is that domain object, the emissions analog of a single `micm::Process`;
// you add one per inventory to an `EmissionsBuilder`.
//
// Trailing-underscore field convention is deliberate; see plan §D2
// ("Preserve PR #8's trailing-underscore convention on public POD
// structs").
#pragma once

#include <string>

#include "miem/species_map.hpp"
#include "miem/util/types.hpp"

namespace miem {

enum class SourceMode             { Offline, Online };
enum class SourceType             { Anthropogenic, Fire, Biogenic, Dust, SeaSalt, Lightning };
enum class TemporalInterpolation  { Linear, Nearest, None };
enum class VerticalInjection      { Surface, Plume };
enum class RegriddingType         { None, Scrip };

// Regridding spec, set once on the `EmissionsBuilder`.  v1 accepts only
// `type_ == None`; `EmissionsBuilder::Build()` rejects anything else.
struct Regridding
{
  RegriddingType type_ = RegriddingType::None;
  std::string    weights_file_;  // unused while type_ == None
};

// Description of a single emission source — the unit you add to an
// `EmissionsBuilder`.  This is a plain value type: it carries no open
// files or runtime state.  The builder turns each `Source` into a runtime
// `EmissionSource` (via `SourceFactory`) when you call `Build()`.
//
// Category/hierarchy follows the HEMCO pattern:
//   * Sources in different categories are summed.
//   * Within a category the highest-hierarchy source wins per cell.
// Duplicate (category, hierarchy) pairs are rejected at `Build()` time.
struct Source
{
  std::string             name_;
  SourceMode              mode_                    = SourceMode::Offline;
  SourceType              type_                    = SourceType::Anthropogenic;

  // File pattern with optional {YYYY}{MM}{DD}{HH} tokens.  Already
  // resolved by musica's translator: MIEM receives a concrete path.
  std::string             file_pattern_;

  // Inventory convention.  v1 accepts only "eccad" (case-insensitive).
  // Anything else is `UnknownConvention` at build time.
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

}  // namespace miem
