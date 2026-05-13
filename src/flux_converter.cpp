// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/flux_converter.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace miem {

Real FluxConverter::FluxToTendency(Real flux_kg_m2_s,
                                   Real air_density_kg_m3,
                                   Real layer_thickness_m)
{
  if (air_density_kg_m3 <= Real{ 0 } || layer_thickness_m <= Real{ 0 })
  {
    return Real{ 0 };
  }
  return flux_kg_m2_s / (air_density_kg_m3 * layer_thickness_m);
}

Result<void> FluxConverter::Apply(EmisState&  state,
                                  const Real* air_density,
                                  const Real* layer_thickness,
                                  int         n_atm_elements)
{
  const int         n_sp    = state.n_species_;
  const int         n_cells = state.n_cells_;
  const int         n_vl    = state.n_vert_levels_;
  const std::size_t expect  = static_cast<std::size_t>(n_vl) * n_cells;

  if (static_cast<std::size_t>(n_atm_elements) < expect)
  {
    return Result<void>::Error(
        ErrorCode::CellCountMismatch,
        "FluxConverter: air_density / layer_thickness need at least " +
        std::to_string(expect) + " elements, got " +
        std::to_string(n_atm_elements) + ".");
  }

  // Re-zero the tendency buffer before accumulating.
  std::fill(state.tendency_.begin(), state.tendency_.end(), Real{ 0 });

  for (int isp = 0; isp < n_sp; ++isp)
  {
    int layer = (isp < static_cast<int>(state.injection_layer_.size()))
                    ? state.injection_layer_[isp]
                    : 0;
    if (layer < 0 || layer >= n_vl)
    {
      layer = 0;
    }

    for (int ic = 0; ic < n_cells; ++ic)
    {
      const Real flux = state.surface_flux_.At(isp, ic);
      const Real rho  = air_density[layer * n_cells + ic];
      const Real dz   = layer_thickness[layer * n_cells + ic];

      const std::size_t tend_idx =
          static_cast<std::size_t>(isp) * n_vl * n_cells +
          static_cast<std::size_t>(layer) * n_cells + ic;
      state.tendency_[tend_idx] = FluxToTendency(flux, rho, dz);
    }
  }

#ifdef MIEM_CHECK_MASS_CONSERVATION
  // Column-integral check: tendency × ρ × Δz summed over levels should
  // equal the input surface flux (to within `kMassToleranceFactor *
  // |flux|`).  We only check the injection layer here because the rest
  // is identically zero — but iterate every level so a stray write is
  // also caught.
  for (int isp = 0; isp < n_sp; ++isp)
  {
    for (int ic = 0; ic < n_cells; ++ic)
    {
      Real sum = Real{ 0 };
      for (int lv = 0; lv < n_vl; ++lv)
      {
        const std::size_t tend_idx =
            static_cast<std::size_t>(isp) * n_vl * n_cells +
            static_cast<std::size_t>(lv) * n_cells + ic;
        const Real rho = air_density[lv * n_cells + ic];
        const Real dz  = layer_thickness[lv * n_cells + ic];
        sum += state.tendency_[tend_idx] * rho * dz;
      }
      const Real flux = state.surface_flux_.At(isp, ic);
      const Real tol  = kMassToleranceFactor *
                        std::max(std::abs(flux), Real{ 1 });
      if (std::abs(sum - flux) > tol)
      {
        return Result<void>::Error(
            ErrorCode::MassConservationViolation,
            "FluxConverter: column-integrated tendency (" +
            std::to_string(sum) + ") differs from surface flux (" +
            std::to_string(flux) + ") for species index " +
            std::to_string(isp) + " cell " + std::to_string(ic) + ".");
      }
    }
  }
#endif

  return Result<void>::Ok();
}

}  // namespace miem
