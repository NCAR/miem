#pragma once

#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

struct SpeciesMapping {
  std::string inventory_name;   // Name in the emission inventory
  std::string mechanism_name;   // Name in the chemical mechanism
  Real scaling_factor = 1.0;    // Mass-based scaling factor
};

class SpeciesMap {
 public:
  SpeciesMap() = default;

  // Load mappings from a YAML file
  explicit SpeciesMap(const std::string& yaml_path);

  // Add a single mapping
  void AddMapping(const std::string& inventory_name,
                  const std::string& mechanism_name,
                  Real scaling_factor = 1.0);

  // Apply mappings to transform inventory flux data to mechanism species.
  // inventory_flux: (n_inventory_species * n_cells) input
  // inventory_names: names corresponding to rows of inventory_flux
  // mechanism_flux: (n_mechanism_species * n_cells) output (accumulated)
  // n_cells: number of grid cells
  void Apply(const std::vector<Real>& inventory_flux,
             const std::vector<std::string>& inventory_names,
             std::vector<Real>& mechanism_flux,
             int n_cells) const;

  // Get unique mechanism species names from all mappings
  std::vector<std::string> MechanismSpecies() const;

  // Get unique inventory species names from all mappings
  std::vector<std::string> InventorySpecies() const;

  const std::vector<SpeciesMapping>& Mappings() const { return mappings_; }

  const std::string& MechanismName() const { return mechanism_name_; }

 private:
  std::string mechanism_name_;
  std::vector<SpeciesMapping> mappings_;
};

}  // namespace miem
