// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Interface for an emission source: reports the mechanism species it
// provides and accumulates their flux for a requested time step.
#pragma once

#include <miem/util/types.hpp>

#include <string>
#include <vector>

namespace miem
{

  class EmissionSource
  {
   public:
    virtual ~EmissionSource() = default;

    // Mechanism species this source provides.
    virtual std::vector<std::string> QuerySpecies() const = 0;

    // Update flux data for the requested time step.  `flux_out` is resized
    // to `species_names_out.size() * n_cells` and accumulated (not
    // overwritten) so callers can chain sources.  Throws MiemException on
    // failure (IO for file/time issues, Validation for size mismatches).
    virtual void
    Update(double time_current, int n_cells, std::vector<Real>& flux_out, std::vector<std::string>& species_names_out) = 0;

    // Selected-cell update. IDs are one-based global inventory slots and
    // output follows the caller's order. Implementations that support
    // distributed I/O override this method.
    virtual void UpdateSelected(
        double time_current,
        int global_n_cells,
        const std::vector<int>& selected_global_cell_ids,
        std::vector<Real>& flux_out,
        std::vector<std::string>& species_names_out) = 0;

    virtual const std::string& Name() const = 0;
  };

}  // namespace miem
