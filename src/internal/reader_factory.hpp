// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Single source of truth for which inventory conventions MIEM reads and
// which reader backs each one.  Both the build-time validator
// (EmissionsBuilder::Validate) and the source constructor
// (OfflineEmissionSource) defer here so the supported-convention list
// lives in exactly one place.
#pragma once

#include <memory>
#include <string>

namespace miem
{

  class EmissionFileReader;

  // True if `convention` (case-insensitive) names a reader MIEM can build.
  bool IsSupportedConvention(const std::string& convention);

  // Construct the file reader for an inventory convention (case-insensitive).
  // Throws MiemException (Configuration / UnknownConvention) for an
  // unsupported convention.
  std::unique_ptr<EmissionFileReader> MakeEmissionFileReader(const std::string& convention);

}  // namespace miem
