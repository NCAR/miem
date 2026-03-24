#include "miem/species_map.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <set>

#include <yaml-cpp/yaml.h>

#include "miem/util/error.hpp"

namespace miem {

SpeciesMap::SpeciesMap(const std::string& yaml_path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception& e) {
    throw ConfigError("Failed to load species map: " + yaml_path +
                      " — " + e.what());
  }

  auto species_map_node = root["species_map"];
  if (!species_map_node) {
    throw ConfigError("Missing 'species_map' key in: " + yaml_path);
  }

  if (species_map_node["mechanism"]) {
    mechanism_name_ = species_map_node["mechanism"].as<std::string>();
  }

  auto mappings_node = species_map_node["mappings"];
  if (!mappings_node || !mappings_node.IsSequence()) {
    throw ConfigError("Missing or invalid 'mappings' in: " + yaml_path);
  }

  // Track scaling factor sums per inventory species for warnings
  std::map<std::string, Real> scaling_sums;

  for (const auto& entry : mappings_node) {
    SpeciesMapping mapping;
    mapping.inventory_name = entry["inventory"].as<std::string>();
    mapping.mechanism_name = entry["mechanism"].as<std::string>();
    mapping.scaling_factor = entry["scaling"]
        ? entry["scaling"].as<Real>()
        : 1.0;

    scaling_sums[mapping.inventory_name] += mapping.scaling_factor;
    mappings_.push_back(std::move(mapping));
  }

  // Warn if scaling factors for any inventory species sum > 1.0
  for (const auto& [name, total] : scaling_sums) {
    if (total > 1.0 + 1e-6) {
      std::cerr << "MIEM Warning: scaling factors for inventory species '"
                << name << "' sum to " << total
                << " (>1.0) — mass is being amplified\n";
    }
  }
}

void SpeciesMap::AddMapping(const std::string& inventory_name,
                            const std::string& mechanism_name,
                            Real scaling_factor) {
  mappings_.push_back({inventory_name, mechanism_name, scaling_factor});
}

std::vector<std::string> SpeciesMap::MechanismSpecies() const {
  std::set<std::string> unique;
  for (const auto& m : mappings_) {
    unique.insert(m.mechanism_name);
  }
  return {unique.begin(), unique.end()};
}

std::vector<std::string> SpeciesMap::InventorySpecies() const {
  std::set<std::string> unique;
  for (const auto& m : mappings_) {
    unique.insert(m.inventory_name);
  }
  return {unique.begin(), unique.end()};
}

void SpeciesMap::Apply(const std::vector<Real>& inventory_flux,
                       const std::vector<std::string>& inventory_names,
                       std::vector<Real>& mechanism_flux,
                       int n_cells) const {
  // Build mechanism species index
  auto mech_species = MechanismSpecies();
  std::map<std::string, int> mech_idx;
  for (int i = 0; i < static_cast<int>(mech_species.size()); ++i) {
    mech_idx[mech_species[i]] = i;
  }

  // Build inventory species index
  std::map<std::string, int> inv_idx;
  for (int i = 0; i < static_cast<int>(inventory_names.size()); ++i) {
    inv_idx[inventory_names[i]] = i;
  }

  // Resize and zero output
  mechanism_flux.assign(mech_species.size() * n_cells, 0.0);

  // Apply each mapping
  for (const auto& mapping : mappings_) {
    auto inv_it = inv_idx.find(mapping.inventory_name);
    auto mech_it = mech_idx.find(mapping.mechanism_name);
    if (inv_it == inv_idx.end() || mech_it == mech_idx.end()) {
      continue;  // Skip unmapped species
    }

    int src_row = inv_it->second;
    int dst_row = mech_it->second;

    for (int ic = 0; ic < n_cells; ++ic) {
      mechanism_flux[dst_row * n_cells + ic] +=
          inventory_flux[src_row * n_cells + ic] * mapping.scaling_factor;
    }
  }
}

}  // namespace miem
