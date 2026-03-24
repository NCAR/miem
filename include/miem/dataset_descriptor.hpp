#pragma once

#include <map>
#include <string>

#include "miem/util/types.hpp"

namespace miem {

// Describes how to interpret a non-CES-compliant NetCDF file.
// Fields provide overrides that normalize the file to CES conventions.
struct DatasetDescriptor {
  std::string variable_prefix = "emi_";        // Default CES prefix
  std::string flux_units = "kg m-2 s-1";
  Real unit_conversion_factor = 1.0;           // Multiply raw data by this
  std::string time_dimension = "Time";
  std::string cell_dimension = "nCells";

  // Rename map: original_var_name -> canonical_name
  std::map<std::string, std::string> species_rename;

  // Load descriptor overrides from a YAML file
  static DatasetDescriptor FromYAML(const std::string& yaml_path);

  // Returns the default CES descriptor (no overrides)
  static DatasetDescriptor Default();
};

}  // namespace miem
