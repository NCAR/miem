// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/source_factory.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

#include "miem/source_offline.hpp"

namespace miem {

namespace {

std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

Result<std::unique_ptr<EmissionSource>>
SourceFactory::Create(const SourceConfig& cfg)
{
  if (cfg.mode_ != SourceMode::Offline)
  {
    return Result<std::unique_ptr<EmissionSource>>::Error(
        ErrorCode::OnlineSourcesNotSupported,
        "SourceFactory: '" + cfg.name_ +
        "' uses an online mode not supported in v1.");
  }

  if (ToLower(cfg.convention_) != "eccad")
  {
    return Result<std::unique_ptr<EmissionSource>>::Error(
        ErrorCode::UnknownConvention,
        "SourceFactory: '" + cfg.name_ + "' uses convention '" +
        cfg.convention_ + "' not supported in v1 (only 'eccad').");
  }

  std::unique_ptr<EmissionSource> source =
      std::make_unique<OfflineEmissionSource>(cfg);
  return Result<std::unique_ptr<EmissionSource>>::Ok(std::move(source));
}

}  // namespace miem
