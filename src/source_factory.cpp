// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/source_factory.hpp>
#include <miem/source_offline.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

namespace miem
{

  namespace
  {

    std::string ToLower(std::string s)
    {
      std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return s;
    }

  }  // namespace

  std::unique_ptr<EmissionSource> SourceFactory::Create(const Source& cfg)
  {
    if (cfg.mode_ != SourceMode::Offline)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED,
          "SourceFactory: '" + cfg.name_ + "' uses an online mode not supported in v1.");
    }

    if (ToLower(cfg.convention_) != "eccad")
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION,
          "SourceFactory: '" + cfg.name_ + "' uses convention '" + cfg.convention_ +
              "' not supported in v1 (only 'eccad').");
    }

    return std::make_unique<OfflineEmissionSource>(cfg);
  }

}  // namespace miem
