// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/emissions_builder.hpp>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>

#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

namespace miem {

namespace {

std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

// The v1 invariant checks, run once at Build() time.  Throws on the first
// violation (MICM-aligned: no multi-error accumulation).  Invariants:
//   V1: regridding_.type_ == RegriddingType::None
//   V2: each source convention_ == "eccad" (case-insensitive)
//   V3: each source mode_       == SourceMode::Offline
//   V4: each source vertical_injection_ == VerticalInjection::Surface
//   V5: each source species_map_.Validate() succeeds (scaling <= 1.0)
//   --: duplicate (category_, hierarchy_) across sources is rejected
void EmissionsBuilder::Validate() const
{
  // V1: regridding type must be None.
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
    // V2: convention must be ECCAD.
    if (ToLower(src.convention_) != "eccad")
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION,
          "Source '" + src.name_ + "': unknown convention '" +
          src.convention_ + "' — v1 supports only 'eccad'.");
    }

    // V3: mode must be Offline.
    if (src.mode_ != SourceMode::Offline)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED,
          "Source '" + src.name_ + "': online sources are not "
          "supported in v1.");
    }

    // V4: vertical injection must be Surface.
    if (src.vertical_injection_ != VerticalInjection::Surface)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_VERTICAL_INJECTION,
          "Source '" + src.name_ + "': only "
          "VerticalInjection::Surface is supported in v1.");
    }

    // V5: species map invariants (scaling sum <= 1.0).  Rethrow with the
    // offending source name for context while preserving category/code.
    try
    {
      src.species_map_.Validate();
    }
    catch (const MiemException& e)
    {
      throw MiemException(e.Category(), e.Code(),
                          "Source '" + src.name_ + "' species_map: " +
                          e.what());
    }

    // Duplicate (category, hierarchy).
    auto key = std::make_pair(src.category_, src.hierarchy_);
    if (!cat_hier.insert(key).second)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_DUPLICATE_CATEGORY_HIERARCHY,
          "Duplicate (category=" + std::to_string(src.category_) +
          ", hierarchy=" + std::to_string(src.hierarchy_) +
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
  return Emissions(sources_, n_cells_, n_vert_levels_);
}

}  // namespace miem
