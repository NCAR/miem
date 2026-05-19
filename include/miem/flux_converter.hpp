// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Convert surface emission fluxes (kg/m²/s) into volumetric tendencies
// (kg/kg/s) at a given injection layer using host-supplied air density
// (kg/m³) and layer thickness (m).  Unit contract:
//
//   tendency [kg/kg/s] = flux [kg/m²/s] / ( air_density [kg/m³] *
//                                            layer_thickness [m] )
//
// Optional runtime mass-conservation check (gated by
// MIEM_CHECK_MASS_CONSERVATION, default ON in Debug / OFF in Release):
// verifies that column-integrated tendency × ρ × Δz equals the input
// surface flux within `kMassToleranceFactor × |flux|`.
#pragma once

#include <vector>

#include "miem/emissions_state.hpp"
#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

class FluxConverter
{
 public:
  // Convert `state.surface_flux_` → `state.tendency_` using the supplied
  // atmospheric state.  Both arrays use layout [level * n_cells + cell]
  // and must have exactly n_vert_levels_ * n_cells_ elements.
  //
  // Returns CellCountMismatch on size mismatch; MassConservationViolation
  // when MIEM_CHECK_MASS_CONSERVATION is enabled and the column integral
  // drifts beyond `kMassToleranceFactor`.
  static Result<void> Apply(EmissionsState&  state,
                            const Real* air_density,
                            const Real* layer_thickness,
                            int         n_atm_elements);

  // Per-species, per-cell scalar form.
  static Real FluxToTendency(Real flux_kg_m2_s,
                             Real air_density_kg_m3,
                             Real layer_thickness_m);

  // Maximum allowed relative drift in the mass-conservation check.
  static constexpr Real kMassToleranceFactor = static_cast<Real>(1e-9);
};

}  // namespace miem
