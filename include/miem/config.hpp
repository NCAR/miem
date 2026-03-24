#pragma once

#include <string>
#include <vector>

#include "miem/temporal_interpolator.hpp"
#include "miem/util/types.hpp"

namespace miem {

// Configuration for a single emission source.
//
// Category/hierarchy follows the HEMCO pattern:
//   - Sources in different categories are summed.
//   - Within the same category, the highest-hierarchy source wins per cell.
// This enables regional inventory overrides (e.g., EPA over CEDS for US cells)
// and automated double-count avoidance.
struct SourceConfig {
  std::string name;
  std::string type;                    // "anthropogenic", "fire", "biogenic"
  std::string file_pattern;            // Path with optional {YYYY}{MM}{DD} tokens
  std::string species_map_path;
  InterpolationMode temporal_interpolation = InterpolationMode::kLinear;
  std::string vertical_injection = "surface";
  std::string descriptor_path;         // Optional: for non-SES files

  int category = 0;                    // Category ID for aggregation grouping
  int hierarchy = 1;                   // Priority within category (higher wins)
  std::string sector;                  // Optional label for diagnostics
  Real scaling_factor = 1.0;           // Runtime scaling (e.g., 0.5 = halve)
};

// Top-level MIEM configuration
struct MIEMConfig {
  std::string version;
  std::vector<SourceConfig> sources;

  static MIEMConfig FromYAML(const std::string& yaml_path);
};

}  // namespace miem
