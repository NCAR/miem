// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Factory for the EmissionSource hierarchy.  v1 only supports
// `mode_ == SourceMode::Offline` + `convention_ == "eccad"`; anything
// else throws MiemException (Configuration category) — validation upstream
// should have already filtered these, but the factory remains defensive.
#pragma once

#include <memory>

#include "miem/source_types.hpp"
#include "miem/source.hpp"

namespace miem {

class SourceFactory
{
 public:
  // Returns the constructed source, or throws MiemException (Configuration)
  // for an unsupported mode / convention.
  static std::unique_ptr<EmissionSource> Create(const Source& cfg);
};

}  // namespace miem
