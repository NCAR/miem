#include "miem/emis_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace miem {

EmisState::EmisState(int n_species, int n_cells, int n_vert_levels)
    : n_species(n_species), n_cells(n_cells), n_vert_levels(n_vert_levels) {
  Resize(n_species, n_cells, n_vert_levels);
}

void EmisState::Resize(int n_sp, int n_c, int n_vl) {
  n_species = n_sp;
  n_cells = n_c;
  n_vert_levels = n_vl;

  surface_flux.resize(static_cast<size_t>(n_species) * n_cells, 0.0);
  tendency.resize(
      static_cast<size_t>(n_species) * n_vert_levels * n_cells, 0.0);
  emis_to_chem_idx.resize(n_species, -1);
  injection_layer.resize(n_species, 0);
  species_names.resize(n_species);
}

void EmisState::Zero() {
  std::fill(surface_flux.begin(), surface_flux.end(), 0.0);
  std::fill(tendency.begin(), tendency.end(), 0.0);
  for (auto& [name, flux] : sector_fluxes) {
    std::fill(flux.begin(), flux.end(), 0.0);
  }
}

const std::vector<Real>& EmisState::GetSectorFlux(
    const std::string& sector) const {
  auto it = sector_fluxes.find(sector);
  if (it == sector_fluxes.end()) {
    throw std::out_of_range("No sector flux for: " + sector);
  }
  return it->second;
}

}  // namespace miem
