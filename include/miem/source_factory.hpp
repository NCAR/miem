// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Factory for the EmissionSource hierarchy.  v1 only supports
// `mode_ == SourceMode::Offline` + `convention_ == "eccad"`; anything
// else returns `Result::Error{…}` (validation upstream should have
// already filtered these, but the factory remains defensive).
#pragma once

#include <memory>

#include "miem/config.hpp"
#include "miem/source.hpp"
#include "miem/util/result.hpp"

namespace miem {

class SourceFactory
{
 public:
  static Result<std::unique_ptr<EmissionSource>> Create(const SourceConfig& cfg);
};

}  // namespace miem
