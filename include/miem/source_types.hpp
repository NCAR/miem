// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include <miem/species_map.hpp>
#include <miem/util/types.hpp>

namespace miem {

/// @brief Whether a source reads offline files or is computed online.
enum class SourceMode { Offline, Online };

/// @brief Emission inventory sector a source represents.
enum class SourceType { Anthropogenic, Fire, Biogenic, Dust, SeaSalt, Lightning };

/// @brief Scheme used to interpolate between successive input time slices.
enum class TemporalInterpolation { Linear, Nearest, None };

/// @brief Where a source deposits its emissions in the vertical column.
enum class VerticalInjection { Surface, Plume };

/// @brief Horizontal regridding scheme applied to a source's input grid.
enum class RegriddingType { None, Scrip };

/// @brief Regridding specification, set once on the @c EmissionsBuilder.
///
/// v1 accepts only @c type_ equal to @c RegriddingType::None;
/// @c EmissionsBuilder::Build() rejects any other value.
struct Regridding
{
  RegriddingType type_ = RegriddingType::None;  ///< Regridding scheme; only @c None in v1.
  std::string    weights_file_;                 ///< Weights path; unused while @c type_ is @c None.
};

/// @brief Description of a single emission source, the unit added to an
///        @c EmissionsBuilder.
///
/// MIEM mirrors MICM: there is no aggregate "config" object. A host (or
/// musica's translator) hands the runtime a collection of domain objects,
/// and @c Source is that object -- the emissions analog of a single
/// @c micm::Process. Add one per inventory to an @c EmissionsBuilder.
///
/// A @c Source is a plain value type: it holds no open files or runtime
/// state. The builder turns each @c Source into a runtime @c EmissionSource
/// (via @c SourceFactory) when @c EmissionsBuilder::Build() is called. All
/// field names carry a trailing underscore.
///
/// Category and hierarchy follow the HEMCO pattern:
/// - Sources in different categories are summed.
/// - Within a category, the highest-hierarchy source wins per cell.
///
/// Duplicate (category, hierarchy) pairs are rejected at @c Build() time.
struct Source
{
  std::string name_;                              ///< Diagnostic name for the source.
  SourceMode  mode_ = SourceMode::Offline;        ///< Offline files vs online computation.
  SourceType  type_ = SourceType::Anthropogenic;  ///< Inventory sector.

  /// File pattern with optional @c {YYYY}{MM}{DD}{HH} tokens. Already
  /// resolved by musica's translator: MIEM receives a concrete path.
  std::string file_pattern_;

  /// Inventory convention. v1 accepts only "eccad" (case-insensitive);
  /// anything else is @c UnknownConvention at build time.
  std::string convention_ = "eccad";

  /// Programmatic species map (musica builds this by translating the
  /// parsed configuration's named @c species_map entry).
  SpeciesMap species_map_;

  /// @brief Interpolation between successive input time slices.
  TemporalInterpolation temporal_interpolation_ = TemporalInterpolation::Linear;

  /// @brief Vertical placement of the emitted mass.
  VerticalInjection vertical_injection_ = VerticalInjection::Surface;

  int  category_       = 0;    ///< HEMCO category index.
  int  hierarchy_      = 1;    ///< HEMCO hierarchy level.
  Real scaling_factor_ = 1.0;  ///< Multiplicative flux scaling.

  std::string sector_;  ///< Optional diagnostic label.
};

}  // namespace miem
