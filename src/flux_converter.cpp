// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/flux_converter.hpp>
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace miem
{

  Real FluxConverter::FluxToTendency(Real flux_kg_m2_s, Real air_density_kg_m3, Real layer_thickness_m)
  {
    if (air_density_kg_m3 <= Real{ 0 } || layer_thickness_m <= Real{ 0 })
    {
      return Real{ 0 };
    }
    return flux_kg_m2_s / (air_density_kg_m3 * layer_thickness_m);
  }

  void FluxConverter::Apply(EmissionsState& state, const Real* air_density, const Real* layer_thickness, int n_atm_elements)
  {
    const int n_sp = state.n_species_;
    const int n_cells = state.n_cells_;
    const int n_vl = state.n_vert_levels_;
    const std::size_t expect = static_cast<std::size_t>(n_vl) * n_cells;

    if (static_cast<std::size_t>(n_atm_elements) < expect)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_VALIDATION,
          MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH,
          "FluxConverter: air_density / layer_thickness need at least " + std::to_string(expect) + " elements, got " +
              std::to_string(n_atm_elements) + ".");
    }

    const std::size_t expected_layer_flux = static_cast<std::size_t>(n_sp) * expect;
    if (state.has_layer_flux_ && state.layer_flux_.size() != expected_layer_flux)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_VALIDATION,
          MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH,
          "FluxConverter: layer_flux needs " + std::to_string(expected_layer_flux) + " elements, got " +
              std::to_string(state.layer_flux_.size()) + ".");
    }

    // Re-zero the tendency buffer before accumulating.
    std::fill(state.tendency_.begin(), state.tendency_.end(), Real{ 0 });

    if (state.has_layer_flux_)
    {
      for (int isp = 0; isp < n_sp; ++isp)
      {
        for (int layer = 0; layer < n_vl; ++layer)
        {
          for (int ic = 0; ic < n_cells; ++ic)
          {
            const std::size_t flux_idx =
                (static_cast<std::size_t>(isp) * n_vl + layer) * n_cells + ic;
            const std::size_t atm_idx = static_cast<std::size_t>(layer) * n_cells + ic;
            state.tendency_[flux_idx] =
                FluxToTendency(state.layer_flux_[flux_idx], air_density[atm_idx], layer_thickness[atm_idx]);
          }
        }
      }
    }
    else
    {
      for (int isp = 0; isp < n_sp; ++isp)
      {
        int layer = (isp < static_cast<int>(state.injection_layer_.size())) ? state.injection_layer_[isp] : 0;
        if (layer < 0 || layer >= n_vl)
        {
          layer = 0;
        }

        for (int ic = 0; ic < n_cells; ++ic)
        {
          const Real flux = state.surface_flux_.At(isp, ic);
          const Real rho = air_density[(layer * n_cells) + ic];
          const Real dz = layer_thickness[(layer * n_cells) + ic];

          const std::size_t tend_idx =
              (static_cast<std::size_t>(isp) * n_vl * n_cells) + (static_cast<std::size_t>(layer) * n_cells) + ic;
          state.tendency_[tend_idx] = FluxToTendency(flux, rho, dz);
        }
      }
    }

#ifdef MIEM_CHECK_MASS_CONSERVATION
    // Column-integral check: tendency × ρ × Δz summed over levels should
    // equal the input surface flux (to within `kMassToleranceFactor *
    // |flux|`). Iterate every level so both fixed profiles and legacy
    // single-layer injection are checked by the same invariant.
    for (int isp = 0; isp < n_sp; ++isp)
    {
      for (int ic = 0; ic < n_cells; ++ic)
      {
        Real sum = Real{ 0 };
        for (int lv = 0; lv < n_vl; ++lv)
        {
          const std::size_t tend_idx =
              (static_cast<std::size_t>(isp) * n_vl * n_cells) + (static_cast<std::size_t>(lv) * n_cells) + ic;
          const Real rho = air_density[(lv * n_cells) + ic];
          const Real dz = layer_thickness[(lv * n_cells) + ic];
          sum += state.tendency_[tend_idx] * rho * dz;
        }
        const Real flux = state.surface_flux_.At(isp, ic);
        // Relative + absolute tolerance.  The previous `max(|flux|, 1)`
        // floor made the check vacuous at realistic 1e-10 kg/m^2/s flux
        // magnitudes; the additive 1e-18 absolute floor keeps the
        // all-zero case from tripping on FP noise without poisoning the
        // relative tolerance at typical magnitudes.
        const Real tol = (kMassToleranceFactor * std::abs(flux)) + Real{ 1e-18 };
        if (std::abs(sum - flux) > tol)
        {
          throw MiemException(
              MIEM_ERROR_CATEGORY_VALIDATION,
              MIEM_VALIDATION_ERROR_CODE_MASS_CONSERVATION,
              "FluxConverter: column-integrated tendency (" + std::to_string(sum) + ") differs from surface flux (" +
                  std::to_string(flux) + ") for species index " + std::to_string(isp) + " cell " + std::to_string(ic) + ".");
        }
      }
    }
#endif
  }

}  // namespace miem
