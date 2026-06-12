// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Builds the concrete EmissionSource for a given Source description, or
// throws MiemException (Configuration) if its mode/convention isn't
// supported.
#pragma once

#include <memory>

#include <miem/source_types.hpp>
#include <miem/source.hpp>

namespace miem {

class SourceFactory
{
 public:
  // Returns the constructed source, or throws MiemException (Configuration)
  // for an unsupported mode / convention.
  static std::unique_ptr<EmissionSource> Create(const Source& cfg);
};

}  // namespace miem
