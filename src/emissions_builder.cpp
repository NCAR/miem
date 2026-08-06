// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "internal/reader_factory.hpp"

#include <miem/emissions_builder.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace miem
{

  // The invariant checks, run once at Build() time.  Throws on the first
  // violation (MICM-aligned: no multi-error accumulation).  Invariants:
  //   - regridding must be RegriddingType::None
  //   - each source's convention must be supported ("eccad" or "uptempo",
  //     case-insensitive)
  //   - each source's mode must be SourceMode::Offline
  //   - vertical injection is Surface or a normalized fixed Profile; Plume
  //     remains unsupported
  //   - each source's species map must validate (scaling sum <= 1.0)
  //   - (category, hierarchy) must be unique across sources
  void EmissionsBuilder::Validate() const
  {
    if (n_cells_ < 1 || n_vert_levels_ < 1)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_VALIDATION,
          MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH,
          "EmissionsBuilder: global cell and vertical-level counts must be positive.");
    }

    std::set<int> selected_ids;
    for (const int global_id : cell_selection_.global_cell_ids_)
    {
      if (global_id < 1 || global_id > n_cells_)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_VALIDATION,
            MIEM_VALIDATION_ERROR_CODE_INVALID_CELL_SELECTION,
            "EmissionsBuilder: selected global cell ID " + std::to_string(global_id) + " is outside 1.." +
                std::to_string(n_cells_) + ".");
      }
      if (!selected_ids.insert(global_id).second)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_VALIDATION,
            MIEM_VALIDATION_ERROR_CODE_INVALID_CELL_SELECTION,
            "EmissionsBuilder: duplicate selected global cell ID " + std::to_string(global_id) + ".");
      }
    }

    // Regridding type must be None.
    if (regridding_.type_ != RegriddingType::None)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_REGRIDDING,
          "EmissionsBuilder: only Regridding::None is supported in v1.");
    }

    // Track (category, hierarchy) to flag duplicates.
    std::set<std::pair<int, int>> cat_hier;
    std::set<std::string> known_sectors;
    std::set<int> known_categories;
    std::set<std::string> mechanism_species;

    for (const auto& src : sources_)
    {
      // Convention must name a reader MIEM can build.
      if (!IsSupportedConvention(src.convention_))
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION,
            "Source '" + src.name_ + "': unknown convention '" + src.convention_ + "' — v1 supports 'eccad' and 'uptempo'.");
      }

      // Mode must be Offline.
      if (src.mode_ != SourceMode::Offline)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED,
            "Source '" + src.name_ +
                "': online sources are not "
                "supported in v1.");
      }

      if (src.vertical_injection_ == VerticalInjection::Plume)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_VERTICAL_INJECTION,
            "Source '" + src.name_ +
                "': VerticalInjection::Plume is not supported; use Surface or a fixed Profile.");
      }
      if (src.vertical_injection_ == VerticalInjection::Surface && !src.vertical_profile_.empty())
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_INVALID_VERTICAL_PROFILE,
            "Source '" + src.name_ + "': vertical_profile is only valid with VerticalInjection::Profile.");
      }
      if (src.vertical_injection_ == VerticalInjection::Profile)
      {
        if (src.vertical_profile_.size() != static_cast<std::size_t>(n_vert_levels_))
        {
          throw MiemException(
              MIEM_ERROR_CATEGORY_CONFIGURATION,
              MIEM_CONFIGURATION_ERROR_CODE_INVALID_VERTICAL_PROFILE,
              "Source '" + src.name_ + "': vertical_profile has " +
                  std::to_string(src.vertical_profile_.size()) + " entries but the grid has " +
                  std::to_string(n_vert_levels_) + " vertical levels.");
        }
        double sum = 0.0;
        for (const double fraction : src.vertical_profile_)
        {
          if (!std::isfinite(fraction) || fraction < 0.0)
          {
            throw MiemException(
                MIEM_ERROR_CATEGORY_CONFIGURATION,
                MIEM_CONFIGURATION_ERROR_CODE_INVALID_VERTICAL_PROFILE,
                "Source '" + src.name_ + "': vertical_profile values must be finite and nonnegative.");
          }
          sum += fraction;
        }
        if (std::abs(sum - 1.0) > 1.0e-12)
        {
          throw MiemException(
              MIEM_ERROR_CATEGORY_CONFIGURATION,
              MIEM_CONFIGURATION_ERROR_CODE_INVALID_VERTICAL_PROFILE,
              "Source '" + src.name_ + "': vertical_profile fractions must sum to one; got " +
                  std::to_string(sum) + ".");
        }
      }

      // Species map invariants (scaling sum <= 1.0).  Rethrow with the
      // offending source name for context while preserving category/code.
      try
      {
        src.species_map_.Validate();
      }
      catch (const MiemException& e)
      {
        throw MiemException(e.Category(), e.Code(), "Source '" + src.name_ + "' species_map: " + e.what());
      }

      // Duplicate (category, hierarchy).
      auto key = std::make_pair(src.category_, src.hierarchy_);
      if (!cat_hier.insert(key).second)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_DUPLICATE_CATEGORY_HIERARCHY,
            "Duplicate (category=" + std::to_string(src.category_) + ", hierarchy=" + std::to_string(src.hierarchy_) +
                ") at source '" + src.name_ + "'.");
      }
      if (!src.sector_.empty())
      {
        known_sectors.insert(src.sector_);
      }
      known_categories.insert(src.category_);
      const auto source_species = src.species_map_.MechanismSpecies();
      mechanism_species.insert(source_species.begin(), source_species.end());
    }

    std::set<std::string> selected_sectors;
    for (const auto& sector : diagnostic_selection_.sectors_)
    {
      if (sector.empty() || !selected_sectors.insert(sector).second)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_INVALID_DIAGNOSTIC_SELECTION,
            "EmissionsBuilder: diagnostic sector names must be nonempty and unique.");
      }
      if (!known_sectors.contains(sector))
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_INVALID_DIAGNOSTIC_SELECTION,
            "EmissionsBuilder: unknown diagnostic sector '" + sector + "'.");
      }
    }

    std::set<int> selected_categories;
    for (const int category : diagnostic_selection_.categories_)
    {
      if (!selected_categories.insert(category).second)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_INVALID_DIAGNOSTIC_SELECTION,
            "EmissionsBuilder: diagnostic category IDs must be unique.");
      }
      if (!known_categories.contains(category))
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_CONFIGURATION,
            MIEM_CONFIGURATION_ERROR_CODE_INVALID_DIAGNOSTIC_SELECTION,
            "EmissionsBuilder: unknown diagnostic category " + std::to_string(category) + ".");
      }
    }

    const std::size_t group_count = selected_sectors.size() + selected_categories.size();
    std::size_t requested_fields = mechanism_species.size();
    if (group_count != 0 && requested_fields > std::numeric_limits<std::size_t>::max() / group_count)
    {
      requested_fields = std::numeric_limits<std::size_t>::max();
    }
    else
    {
      requested_fields *= group_count;
    }
    if (diagnostic_selection_.layered_output_)
    {
      const std::size_t levels = static_cast<std::size_t>(n_vert_levels_);
      if (levels != 0 && requested_fields > std::numeric_limits<std::size_t>::max() / levels)
      {
        requested_fields = std::numeric_limits<std::size_t>::max();
      }
      else
      {
        requested_fields *= levels;
      }
    }
    if (requested_fields > diagnostic_selection_.max_fields_)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_INVALID_DIAGNOSTIC_SELECTION,
          "EmissionsBuilder: diagnostic selection requests " + std::to_string(requested_fields) +
              " fields, exceeding max_fields=" + std::to_string(diagnostic_selection_.max_fields_) + ".");
    }
  }

  Emissions EmissionsBuilder::Build() const
  {
    Validate();  // throws MiemException on an invalid configuration

    // Emissions' private constructor compiles the Source list into runtime
    // sources via SourceFactory, throwing MiemException (tagged with the
    // source name) if any source is rejected.  EmissionsBuilder is a friend.
    return Emissions(sources_, n_cells_, n_vert_levels_, cell_selection_, diagnostic_selection_);
  }

}  // namespace miem
