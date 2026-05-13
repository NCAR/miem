// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/emis_state.hpp"

#include <algorithm>

namespace miem {

void EmisState::Resize(int n_species, int n_cells, int n_vert_levels)
{
  n_species_     = n_species;
  n_cells_       = n_cells;
  n_vert_levels_ = n_vert_levels;

  surface_flux_.Resize(n_species, n_cells);
  tendency_.assign(static_cast<std::size_t>(n_species) * n_vert_levels * n_cells,
                   Real{ 0 });
  emis_to_chem_idx_.assign(n_species, -1);
  injection_layer_.assign(n_species, 0);
  species_names_.assign(n_species, std::string{});
}

void EmisState::Zero()
{
  std::fill(surface_flux_.raw().begin(), surface_flux_.raw().end(),
            Real{ 0 });
  std::fill(tendency_.begin(), tendency_.end(), Real{ 0 });
  for (auto& [name, flux] : sector_fluxes_)
  {
    std::fill(flux.raw().begin(), flux.raw().end(), Real{ 0 });
  }
}

const FluxArray* EmisState::GetSectorFlux(const std::string& sector) const
{
  auto it = sector_fluxes_.find(sector);
  if (it == sector_fluxes_.end())
  {
    return nullptr;
  }
  return &it->second;
}

}  // namespace miem
