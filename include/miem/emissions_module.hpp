// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `EmissionsModule` is the runtime entry point for MIEM.  Construct with a
// pre-validated `MIEMConfig` plus host grid dimensions; call `Run` once
// per time step to obtain an `EmisState` snapshot of surface flux, per-
// sector flux, and (optionally) volumetric tendency.
//
// The constructor is noexcept-on-pre-validated-config: schema invariants
// are checked once by `MIEMConfig::Validate()` and assumed to hold.
// Runtime failures (NetCDF I/O, cell-count mismatch with host arrays,
// time-out-of-range) surface as `Result::Error{…}` from `Run`.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miem/config.hpp"
#include "miem/emis_state.hpp"
#include "miem/source.hpp"
#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

class EmissionsModule
{
 public:
  // Preconditions: `cfg` has passed `MIEMConfig::Validate()`.
  // Construction itself is fallible (source factories may reject) so
  // callers should prefer the static `Create` factory below.
  EmissionsModule(const MIEMConfig& cfg, int n_cells, int n_vert_levels);

  // Convenience factory that runs `cfg.Validate()` first and bundles
  // construction errors into a single `Result<EmissionsModule>`.
  static Result<std::unique_ptr<EmissionsModule>>
  Create(const MIEMConfig& cfg, int n_cells, int n_vert_levels);

  // Aggregate mechanism species across all sources.
  std::vector<std::string> QuerySpecies() const { return aggregated_species_; }

  // Surface-flux-only run (no tendency conversion).  Returned EmisState
  // has `surface_flux_` and `sector_fluxes_` populated; `tendency_` is
  // zero-filled.
  Result<EmisState> Run(double sim_time_sec, double dt_sec);

  // Full run with tendency conversion.  Both `air_density` and
  // `layer_thickness` are (n_vert_levels * n_cells) flat arrays
  // (layout: [level * n_cells + cell]).
  Result<EmisState> Run(double      sim_time_sec,
                        double      dt_sec,
                        const Real* air_density,
                        const Real* layer_thickness,
                        int         n_atm_elements);

  // Map this module's mechanism species onto host indices, -1 when the
  // host does not provide a given species.
  void ResolveHostIndices(const std::vector<std::string>& host_species,
                          std::vector<int>&               indices) const;

  int NumSpecies()    const { return static_cast<int>(aggregated_species_.size()); }
  int NumCells()      const { return n_cells_; }
  int NumVertLevels() const { return n_vert_levels_; }
  const std::vector<std::string>& SpeciesNames() const { return aggregated_species_; }

 private:
  struct SourceEntry
  {
    std::unique_ptr<EmissionSource> source_;
    int                             category_;
    int                             hierarchy_;
    std::string                     sector_;
    Real                            scaling_factor_;
  };

  std::vector<SourceEntry> sources_;
  std::vector<std::string> aggregated_species_;
  int                      n_cells_;
  int                      n_vert_levels_;

  // Populate sources_ + aggregated_species_ from cfg.  Returns a
  // populated error list when any source fails to construct.
  std::vector<ErrorEntry> BuildSources(const MIEMConfig& cfg);
};

}  // namespace miem
