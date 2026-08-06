// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "internal/reader_factory.hpp"

#include <miem/emissions_builder.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <cmath>
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
    }
  }

  Emissions EmissionsBuilder::Build() const
  {
    Validate();  // throws MiemException on an invalid configuration

    // Emissions' private constructor compiles the Source list into runtime
    // sources via SourceFactory, throwing MiemException (tagged with the
    // source name) if any source is rejected.  EmissionsBuilder is a friend.
    return Emissions(sources_, n_cells_, n_vert_levels_, cell_selection_);
  }

}  // namespace miem
