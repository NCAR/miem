#pragma once

#include <map>
#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

struct EmisState {
  int n_species = 0;
  int n_cells = 0;
  int n_vert_levels = 0;

  // Surface fluxes: (n_species * n_cells) in kg/m^2/s
  // Layout: species-major, i.e. flux[species_idx * n_cells + cell_idx]
  std::vector<Real> surface_flux;

  // Volumetric tendencies: (n_species * n_vert_levels * n_cells) in kg/kg/s
  // Layout: tendency[species_idx * n_vert_levels * n_cells + level_idx * n_cells + cell_idx]
  std::vector<Real> tendency;

  // Maps emission species index to host chemistry species index
  std::vector<int> emis_to_chem_idx;

  // Injection layer for each species (0 = surface)
  std::vector<int> injection_layer;

  // Species names for this emission state
  std::vector<std::string> species_names;

  // Per-sector surface fluxes: sector_name -> (n_species * n_cells) array
  std::map<std::string, std::vector<Real>> sector_fluxes;

  // Sector names in insertion order
  std::vector<std::string> sector_names;

  EmisState() = default;
  EmisState(int n_species, int n_cells, int n_vert_levels);

  void Resize(int n_species, int n_cells, int n_vert_levels);
  void Zero();

  bool HasSectors() const { return !sector_fluxes.empty(); }
  const std::vector<Real>& GetSectorFlux(const std::string& sector) const;

  Real* SurfaceFluxData() { return surface_flux.data(); }
  const Real* SurfaceFluxData() const { return surface_flux.data(); }

  Real* TendencyData() { return tendency.data(); }
  const Real* TendencyData() const { return tendency.data(); }

  int* EmisToChemIdxData() { return emis_to_chem_idx.data(); }
  const int* EmisToChemIdxData() const { return emis_to_chem_idx.data(); }
};

}  // namespace miem
