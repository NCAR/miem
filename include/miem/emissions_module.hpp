#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miem/emis_state.hpp"
#include "miem/source.hpp"
#include "miem/util/types.hpp"

namespace miem {

class EmissionsModule {
 public:
  // Phase 1: Discovery — query available species before constructing
  static std::vector<std::string> QuerySpecies(const std::string& config_path);

  // Phase 2: Construction
  EmissionsModule(const std::string& config_path, int n_cells, int n_vert_levels);

  // Resolve emission species to host chemistry indices.
  // host_species: list of species names known to the host model
  // indices: output, maps each emission species to its host index (-1 if not found)
  void ResolveHostIndices(const std::vector<std::string>& host_species,
                          std::vector<int>& indices) const;

  // Run emissions for a time step, returning an EmisState.
  // air_density: (n_vert_levels * n_cells) in kg/m^3
  // layer_thickness: (n_vert_levels * n_cells) in meters
  EmisState Run(double time_current, double dt,
                const std::vector<Real>& air_density,
                const std::vector<Real>& layer_thickness);

  int NumSpecies() const { return static_cast<int>(aggregated_species_.size()); }
  int NumCells() const { return n_cells_; }
  int NumVertLevels() const { return n_vert_levels_; }
  const std::vector<std::string>& SpeciesNames() const { return aggregated_species_; }

 private:
  std::vector<std::unique_ptr<EmissionSource>> sources_;
  std::vector<std::string> aggregated_species_;
  int n_cells_;
  int n_vert_levels_;
};

}  // namespace miem
