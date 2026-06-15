// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `Emissions` is the runtime entry point for MIEM.  You do not construct
// it directly: assemble the `Source` descriptions with an
// `EmissionsBuilder` (see emissions_builder.hpp) and call `Build()`, which
// validates the configuration and compiles the sources into a module.
// Call `Run` once per time step to obtain an `EmissionsState` snapshot of
// surface flux, per-sector flux, and (optionally) volumetric tendency.
//
// The module is move-only: it owns the open source readers.  Schema
// invariants are checked once by `EmissionsBuilder::Build()`; runtime
// failures (NetCDF I/O, cell-count mismatch with host arrays,
// time-out-of-range) are raised as `MiemException` from `Run`.
#pragma once

#include <miem/emissions_state.hpp>
#include <miem/source.hpp>
#include <miem/source_types.hpp>
#include <miem/util/types.hpp>

#include <memory>
#include <string>
#include <vector>

namespace miem
{

  // Builds and validates an Emissions module from a set of Source
  // descriptions; the only thing that may construct an Emissions.
  class EmissionsBuilder;

  class Emissions
  {
   public:
    // Move-only: an Emissions owns its sources' open readers.
    Emissions(Emissions&&) noexcept = default;
    Emissions& operator=(Emissions&&) noexcept = default;
    Emissions(const Emissions&) = delete;
    Emissions& operator=(const Emissions&) = delete;

    // Aggregate mechanism species across all sources.
    std::vector<std::string> QuerySpecies() const
    {
      return aggregated_species_;
    }

    // Surface-flux-only run (no tendency conversion).  Returned EmissionsState
    // has `surface_flux_` and `sector_fluxes_` populated; `tendency_` is
    // zero-filled.  Throws MiemException on a runtime failure.
    EmissionsState Run(double sim_time_sec, double dt_sec);

    // Full run with tendency conversion.  Both `air_density` and
    // `layer_thickness` are (n_vert_levels * n_cells) flat arrays
    // (layout: [level * n_cells + cell]).  Throws MiemException on failure.
    EmissionsState
    Run(double sim_time_sec, double dt_sec, const Real* air_density, const Real* layer_thickness, int n_atm_elements);

    // Map this module's mechanism species onto host indices, -1 when the
    // host does not provide a given species.
    void ResolveHostIndices(const std::vector<std::string>& host_species, std::vector<int>& indices) const;

    int NumSpecies() const
    {
      return static_cast<int>(aggregated_species_.size());
    }
    int NumCells() const
    {
      return n_cells_;
    }
    int NumVertLevels() const
    {
      return n_vert_levels_;
    }
    const std::vector<std::string>& SpeciesNames() const
    {
      return aggregated_species_;
    }

   private:
    friend class EmissionsBuilder;

    // Constructed only by `EmissionsBuilder::Build()` from an already-
    // validated `Source` list.  Throws MiemException (tagged with the
    // offending source's name) if a source factory rejects its description.
    Emissions(const std::vector<Source>& sources, int n_cells, int n_vert_levels);

    struct SourceEntry
    {
      std::unique_ptr<EmissionSource> source_;
      int category_;
      int hierarchy_;
      std::string sector_;
      Real scaling_factor_;
    };

    std::vector<SourceEntry> sources_;
    std::vector<std::string> aggregated_species_;
    int n_cells_;
    int n_vert_levels_;

    // Populate sources_ + aggregated_species_ from the Source list.  Throws
    // MiemException (tagged with the offending source's name) if a source
    // factory rejects its description.
    void BuildSources(const std::vector<Source>& sources);
  };

}  // namespace miem
