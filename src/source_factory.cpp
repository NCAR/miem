// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/source_factory.hpp>
#include <miem/source_offline.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <memory>

namespace miem
{

  std::unique_ptr<EmissionSource> SourceFactory::Create(const Source& cfg)
  {
    if (cfg.mode_ != SourceMode::Offline)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_CONFIGURATION,
          MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED,
          "SourceFactory: '" + cfg.name_ + "' uses an online mode not supported in v1.");
    }

    // The inventory convention selects the file reader; OfflineEmissionSource
    // builds it and throws MiemException (UnknownConvention) for an
    // unsupported convention.
    return std::make_unique<OfflineEmissionSource>(cfg);
  }

}  // namespace miem
