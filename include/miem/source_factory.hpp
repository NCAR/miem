#pragma once

#include <memory>

#include "miem/config.hpp"
#include "miem/source.hpp"

namespace miem {

// Factory for creating EmissionSource instances from configuration.
// Decouples EmissionsModule from concrete source types (OCP).
class SourceFactory {
 public:
  // Create the appropriate EmissionSource subclass based on config.
  // Currently supports offline sources; extensible for online sources
  // (dust, sea salt, biogenic) in future versions.
  static std::unique_ptr<EmissionSource> Create(const SourceConfig& config);
};

}  // namespace miem
