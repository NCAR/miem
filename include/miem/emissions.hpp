// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Interim placeholder.  The full runtime module -- constructed through
// `EmissionsBuilder` and backed by the `EmissionSource` hierarchy -- lands
// in the runtime-integration slice.  Until then this stub only holds the
// `Source` descriptions so the description types can be exercised in
// isolation.
#pragma once

#include <utility>
#include <vector>

#include <miem/source_types.hpp>
#include <miem/emissions_state.hpp>

namespace miem {

class Emissions
{
 public:
  Emissions(std::vector<Source> sources, int n_cells, int n_vert_levels)
      : sources_(std::move(sources)),
        n_cells_(n_cells),
        n_vert_levels_(n_vert_levels)
  {
  }

  int NumSources() const { return static_cast<int>(sources_.size()); }

  EmissionsState Run(double /*sim_time_sec*/, double /*dt_sec*/)
  {
    EmissionsState state;
    for (const auto& source : sources_)
      if (!source.sector_.empty())
        state.sector_fluxes_.emplace(source.sector_, EmissionsArray{});
    return state;
  }

 private:
  std::vector<Source> sources_;
  int n_cells_;
  int n_vert_levels_;
};

}  // namespace miem
