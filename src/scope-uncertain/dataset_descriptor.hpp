// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// NOT FOR v1 USE.  See ./README.md.  In v1 only `Inventory.convention =
// "eccad"` is accepted and `MIEMConfig::Validate()` rejects anything that
// would route through `DatasetDescriptor`.  This file is preserved verbatim
// from `feature/scaffolding` so the v1.1 resurrection is mechanical.

#pragma once

#include <map>
#include <string>

#include "miem/util/types.hpp"

namespace miem {

// Describes how to interpret a non-ECCAD-compliant NetCDF file.
// Fields provide overrides that normalize the file to ECCAD conventions.
struct DatasetDescriptor
{
  std::string variable_prefix_      = "emi_";       // Default ECCAD prefix
  std::string flux_units_           = "kg m-2 s-1";
  Real        unit_conversion_factor_ = 1.0;        // Multiply raw data by this
  std::string time_dimension_       = "time";
  std::string cell_dimension_       = "n_cells";
  std::string grid_description_;

  // Rename map: original_var_name -> canonical_name
  std::map<std::string, std::string> species_rename_;

  // Returns the default ECCAD descriptor (no overrides)
  static DatasetDescriptor Default() { return DatasetDescriptor{}; }
};

}  // namespace miem
