// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `EmisState` is the value returned by `EmissionsModule::Run`.  It holds
// the per-cell, per-species surface flux and volumetric tendency arrays,
// plus optional per-sector diagnostic fluxes.  All field names carry a
// trailing underscore per plan §D2.
//
// Memory layout:
//   surface_flux_ : species-major, [species_idx * n_cells_ + cell_idx]
//   tendency_     : species + level major,
//                   [species_idx * n_vert_levels_ * n_cells_ +
//                    level_idx   * n_cells_      + cell_idx]
//
// Convenience accessors `operator()(cell, species)` resolve by species
// name via an index map maintained alongside the species list.  Raw
// pointer accessors are exposed for the zero-copy C API hand-off.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

// Two-dimensional view backed by a flat std::vector<Real>: rows are
// species, columns are cells.  Stored in species-major (row-major)
// order so `&data()[species_idx * n_cells_]` is the start of a row.
class FluxArray
{
 public:
  FluxArray() = default;
  FluxArray(int n_species, int n_cells)
      : n_species_(n_species),
        n_cells_(n_cells),
        data_(static_cast<std::size_t>(n_species) * n_cells, Real{ 0 })
  {
  }

  void Resize(int n_species, int n_cells)
  {
    n_species_ = n_species;
    n_cells_   = n_cells;
    data_.assign(static_cast<std::size_t>(n_species) * n_cells, Real{ 0 });
  }

  void SetSpecies(const std::vector<std::string>& names)
  {
    species_names_ = names;
    species_index_.clear();
    for (int i = 0; i < static_cast<int>(names.size()); ++i)
    {
      species_index_[names[i]] = i;
    }
  }

  Real operator()(int cell, const std::string& species) const
  {
    auto it = species_index_.find(species);
    if (it == species_index_.end() || cell < 0 || cell >= n_cells_)
    {
      return Real{ 0 };
    }
    return data_[static_cast<std::size_t>(it->second) * n_cells_ + cell];
  }

  Real& At(int species_idx, int cell)
  {
    return data_[static_cast<std::size_t>(species_idx) * n_cells_ + cell];
  }

  Real At(int species_idx, int cell) const
  {
    return data_[static_cast<std::size_t>(species_idx) * n_cells_ + cell];
  }

  int                       n_species() const { return n_species_; }
  int                       n_cells()   const { return n_cells_; }
  Real*                     data()            { return data_.data(); }
  const Real*               data()      const { return data_.data(); }
  std::size_t               size()      const { return data_.size(); }
  const std::vector<Real>&  raw()       const { return data_; }
  std::vector<Real>&        raw()             { return data_; }
  const std::vector<std::string>& SpeciesNames() const { return species_names_; }

 private:
  int                            n_species_ = 0;
  int                            n_cells_   = 0;
  std::vector<Real>              data_;
  std::vector<std::string>       species_names_;
  std::map<std::string, int>     species_index_;
};

struct EmissionsState
{
  int n_species_     = 0;
  int n_cells_       = 0;
  int n_vert_levels_ = 0;

  std::vector<std::string> species_names_;

  // Maps emission species index to host chemistry species index, -1 if
  // not present.  Populated by `EmissionsModule::ResolveHostIndices`.
  std::vector<int> emis_to_chem_idx_;

  // Per-species injection layer (0 = surface).
  std::vector<int> injection_layer_;

  FluxArray surface_flux_;   // (n_species, n_cells)            [kg/m²/s]

  // Volumetric tendency: flat (n_species * n_vert_levels * n_cells).
  std::vector<Real> tendency_;

  // Optional diagnostic per-sector fluxes (same shape as surface_flux_).
  std::map<std::string, FluxArray> sector_fluxes_;
  std::vector<std::string>         sector_names_;  // insertion order

  EmisState() = default;
  EmisState(int n_species, int n_cells, int n_vert_levels)
  {
    Resize(n_species, n_cells, n_vert_levels);
  }

  void Resize(int n_species, int n_cells, int n_vert_levels);
  void Zero();

  bool HasSectors() const { return !sector_fluxes_.empty(); }

  // Convenience accessor for sector flux by name; returns nullptr if the
  // sector is not present.
  const FluxArray* GetSectorFlux(const std::string& sector) const;

  // Raw-pointer accessors for the C API zero-copy hand-off.
  Real*       SurfaceFluxData()       { return surface_flux_.data(); }
  const Real* SurfaceFluxData() const { return surface_flux_.data(); }
  Real*       TendencyData()          { return tendency_.data(); }
  const Real* TendencyData()    const { return tendency_.data(); }
  int*        EmisToChemIdxData()       { return emis_to_chem_idx_.data(); }
  const int*  EmisToChemIdxData() const { return emis_to_chem_idx_.data(); }
};

}  // namespace miem
