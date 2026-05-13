// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/config.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>

namespace miem {

namespace {

std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

Result<void> MIEMConfig::Validate() const
{
  Result<void> result;

  // V1: regridding type must be None.
  if (regridding_.type_ != RegriddingType::None)
  {
    result.AddError(ErrorCode::UnsupportedRegriddingType,
                    "MIEMConfig: only Regridding::None is supported in v1.");
  }

  // Track (category, hierarchy) to flag duplicates.
  std::set<std::pair<int, int>> cat_hier;

  for (const auto& src : sources_)
  {
    // V2: convention must be ECCAD.
    if (ToLower(src.convention_) != "eccad")
    {
      result.AddError(ErrorCode::UnknownConvention,
                      "Source '" + src.name_ + "': unknown convention '" +
                      src.convention_ + "' — v1 supports only 'eccad'.");
    }

    // V3: mode must be Offline.
    if (src.mode_ != SourceMode::Offline)
    {
      result.AddError(ErrorCode::OnlineSourcesNotSupported,
                      "Source '" + src.name_ + "': online sources are not "
                      "supported in v1.");
    }

    // V4: vertical injection must be Surface.
    if (src.vertical_injection_ != VerticalInjection::Surface)
    {
      result.AddError(ErrorCode::UnsupportedVerticalInjection,
                      "Source '" + src.name_ + "': only "
                      "VerticalInjection::Surface is supported in v1.");
    }

    // V5: species map invariants (scaling sum ≤ 1.0).
    auto sm_result = src.species_map_.Validate();
    if (!sm_result)
    {
      for (const auto& e : sm_result.errors())
      {
        result.AddError(e.code_,
                        "Source '" + src.name_ + "' species_map: " + e.message_);
      }
    }

    // Duplicate (category, hierarchy).
    auto key = std::make_pair(src.category_, src.hierarchy_);
    if (!cat_hier.insert(key).second)
    {
      result.AddError(ErrorCode::DuplicateCategoryHierarchy,
                      "Duplicate (category=" + std::to_string(src.category_) +
                      ", hierarchy=" + std::to_string(src.hierarchy_) +
                      ") at source '" + src.name_ + "'.");
    }
  }

  return result;
}

}  // namespace miem
