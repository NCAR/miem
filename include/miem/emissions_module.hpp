// Copyright (C) 2026 National Center for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <miem/config.hpp>
#include <miem/emis_state.hpp>

namespace miem {

class EmissionsModule
{
 public:
  EmissionsModule(const MIEMConfig& cfg, int n_cells, int n_vert_levels)
      : cfg_(cfg), n_cells_(n_cells), n_vert_levels_(n_vert_levels)
  {
  }

  EmisState Run(double sim_time_sec, double dt_sec)
  {
    EmisState state;
    for (const auto& source : cfg_.sources_)
      if (!source.sector_.empty())
        state.sector_fluxes_.emplace(source.sector_, FluxArray{});
    return state;
  }

 private:
  MIEMConfig cfg_;
  int n_cells_;
  int n_vert_levels_;
};

}  // namespace miem
