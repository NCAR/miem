#pragma once

#include <string>
#include <vector>

#include "miem/temporal_interpolator.hpp"

namespace miem {

// Configuration for a single emission source
struct SourceConfig {
  std::string name;
  std::string type;                    // "anthropogenic", "fire", "biogenic"
  std::string file_pattern;            // Path with optional {YYYY}{MM}{DD} tokens
  std::string species_map_path;
  InterpolationMode temporal_interpolation = InterpolationMode::kLinear;
  std::string vertical_injection = "surface";
  std::string descriptor_path;         // Optional: for non-CES files
};

// Top-level MIEM configuration
struct MIEMConfig {
  std::string version;
  std::vector<SourceConfig> sources;

  static MIEMConfig FromYAML(const std::string& yaml_path);
};

}  // namespace miem
