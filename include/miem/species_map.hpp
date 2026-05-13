// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Programmatic-only species mapping.  Constructed by adding mappings one
// at a time (typically by musica's translator from the parsed YAML), then
// invoked at runtime by `OfflineEmissionSource` to transform inventory
// flux into mechanism flux.  No YAML constructor — schema parsing lives
// in MechanismConfiguration.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

struct SpeciesMapping
{
  std::string inventory_name_;   // name in the emission inventory
  std::string mechanism_name_;   // name in the chemical mechanism
  Real        scaling_factor_ = 1.0;  // mass-based per-mapping scaling
};

class SpeciesMap
{
 public:
  SpeciesMap() = default;

  // Add a single mapping.  Rebuilds the cached mechanism index.
  void AddMapping(const std::string& inventory_name,
                  const std::string& mechanism_name,
                  Real               scaling_factor = 1.0);

  // Validate scaling-factor sums per inventory species (must be ≤ 1.0 +
  // tolerance to avoid silent mass amplification).
  Result<void> Validate() const;

  // Apply the mappings to transform inventory flux to mechanism flux.
  // inventory_flux  : (n_inventory_species * n_cells) input
  // inventory_names : names matching the rows of inventory_flux
  // mechanism_flux  : (n_mechanism_species * n_cells) output (resized,
  //                   zero-initialized, then accumulated)
  Result<void> Apply(const std::vector<Real>&        inventory_flux,
                     const std::vector<std::string>& inventory_names,
                     std::vector<Real>&              mechanism_flux,
                     int                             n_cells) const;

  std::vector<std::string> MechanismSpecies() const;
  std::vector<std::string> InventorySpecies() const;

  const std::vector<SpeciesMapping>& Mappings() const { return mappings_; }

  const std::string& MechanismName() const          { return mechanism_name_; }
  void SetMechanismName(const std::string& name)    { mechanism_name_ = name; }

 private:
  std::string                 mechanism_name_;
  std::vector<SpeciesMapping> mappings_;

  // Cached mechanism index, rebuilt by `AddMapping`.
  std::vector<std::string> cached_mechanism_species_;
  std::map<std::string, int> cached_mech_idx_;

  void RebuildCache();
};

}  // namespace miem
