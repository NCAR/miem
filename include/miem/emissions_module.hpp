#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miem/emis_state.hpp"
#include "miem/source.hpp"
#include "miem/util/types.hpp"

namespace miem {

class MIEMConfig;  // forward declaration

class EmissionsModule {
 public:
  // Phase 1: Discovery — query available species before constructing.
  // Accepts either a file path or a pre-parsed config to avoid double-parsing.
  static std::vector<std::string> QuerySpecies(const std::string& config_path);
  static std::vector<std::string> QuerySpecies(const MIEMConfig& config);

  // Phase 2: Construction
  EmissionsModule(const std::string& config_path, int n_cells, int n_vert_levels);

  // Resolve emission species to host chemistry indices.
  // host_species: list of species names known to the host model
  // indices: output, maps each emission species to its host index (-1 if not found)
  void ResolveHostIndices(const std::vector<std::string>& host_species,
                          std::vector<int>& indices) const;

  // Run emissions for a time step, returning an EmisState.
  // time_current: seconds since epoch
  // air_density: (n_vert_levels * n_cells) in kg/m^3
  // layer_thickness: (n_vert_levels * n_cells) in meters
  // n_atm_elements: size of air_density and layer_thickness arrays
  EmisState Run(double time_current,
                const Real* air_density, const Real* layer_thickness,
                int n_atm_elements);

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
