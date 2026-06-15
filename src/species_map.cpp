// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/species_map.hpp>

#include <map>
#include <set>
#include <string>

#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

namespace miem {

void SpeciesMap::AddMapping(const std::string& inventory_name,
                            const std::string& mechanism_name,
                            Real               scaling_factor)
{
  mappings_.push_back({ inventory_name, mechanism_name, scaling_factor });
  RebuildCache();
}

void SpeciesMap::RebuildCache()
{
  std::set<std::string> unique;
  for (const auto& m : mappings_)
  {
    unique.insert(m.mechanism_name_);
  }
  cached_mechanism_species_ = { unique.begin(), unique.end() };

  cached_mech_idx_.clear();
  for (int i = 0; i < static_cast<int>(cached_mechanism_species_.size()); ++i)
  {
    cached_mech_idx_[cached_mechanism_species_[i]] = i;
  }
}

std::vector<std::string> SpeciesMap::MechanismSpecies() const
{
  return cached_mechanism_species_;
}

std::vector<std::string> SpeciesMap::InventorySpecies() const
{
  std::set<std::string> unique;
  for (const auto& m : mappings_)
  {
    unique.insert(m.inventory_name_);
  }
  return { unique.begin(), unique.end() };
}

void SpeciesMap::Validate() const
{
  std::map<std::string, Real> scaling_sums;
  for (const auto& m : mappings_)
  {
    scaling_sums[m.inventory_name_] += m.scaling_factor_;
  }

  for (const auto& [name, total] : scaling_sums)
  {
    if (total > static_cast<Real>(1.0) + static_cast<Real>(1e-6))
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_SPECIES,
          MIEM_SPECIES_ERROR_CODE_SCALING_EXCEEDS_ONE,
          "Scaling factors for inventory species '" + name + "' sum to " +
          std::to_string(total) +
          " (>1.0) — mass would be amplified.");
    }
  }
}

void SpeciesMap::Apply(const std::vector<Real>&        inventory_flux,
                       const std::vector<std::string>& inventory_names,
                       std::vector<Real>&              mechanism_flux,
                       int                             n_cells) const
{
  // Precondition: inventory_flux is (inventory_names.size() * n_cells).
  const std::size_t expected_in =
      inventory_names.size() * static_cast<std::size_t>(n_cells);
  if (inventory_flux.size() != expected_in)
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_VALIDATION,
        MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH,
        "SpeciesMap::Apply: inventory_flux size " +
        std::to_string(inventory_flux.size()) + " does not match " +
        std::to_string(inventory_names.size()) + " species * " +
        std::to_string(n_cells) + " cells.");
  }

  // Build the inventory species → row index map (caller-defined ordering).
  std::map<std::string, int> inv_idx;
  for (int i = 0; i < static_cast<int>(inventory_names.size()); ++i)
  {
    inv_idx[inventory_names[i]] = i;
  }

  // Resize and zero output, then accumulate the mappings.
  mechanism_flux.assign(cached_mechanism_species_.size() *
                            static_cast<std::size_t>(n_cells),
                        static_cast<Real>(0.0));

  for (const auto& mapping : mappings_)
  {
    auto inv_it  = inv_idx.find(mapping.inventory_name_);
    auto mech_it = cached_mech_idx_.find(mapping.mechanism_name_);
    if (inv_it == inv_idx.end() || mech_it == cached_mech_idx_.end())
    {
      // Inventory species missing from the input frame, or mechanism species
      // missing from the cache — silently drop (matches scaffolding).
      continue;
    }

    const int src_row = inv_it->second;
    const int dst_row = mech_it->second;

    for (int ic = 0; ic < n_cells; ++ic)
    {
      mechanism_flux[dst_row * n_cells + ic] +=
          inventory_flux[src_row * n_cells + ic] * mapping.scaling_factor_;
    }
  }
}

}  // namespace miem
