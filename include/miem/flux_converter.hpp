// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <miem/emissions_state.hpp>
#include <miem/util/types.hpp>

#include <vector>

namespace miem
{

  /// @brief Converts per-layer emission fluxes into volumetric tendencies.
  ///
  /// Converts each layer emission flux [kg m-2 s-1] into a volumetric tendency
  /// [kg kg-1 s-1], using a host-supplied air
  /// density [kg m-3] and layer thickness [m]:
  ///
  /// \f[
  ///   \text{tendency} = \frac{\text{flux}}{\rho_\text{air} \, \Delta z}
  /// \f]
  ///
  /// An optional runtime mass-conservation check (gated by
  /// @c MIEM_CHECK_MASS_CONSERVATION, default ON in Debug / OFF in Release)
  /// verifies that the column-integrated reconstruction
  /// \f$ \sum_k \text{tendency}_k \, \rho_k \, \Delta z_k \f$ equals the input
  /// surface flux to within @c kMassToleranceFactor of \f$ |\text{flux}| \f$.
  class FluxConverter
  {
   public:
    /// @brief Convert @c state.layer_flux_ into @c state.tendency_ in place.
    ///
    /// Both atmospheric arrays use the layout @c [level*n_cells+cell] and must
    /// hold exactly @c n_vert_levels_*n_cells_ elements.
    ///
    /// @param state           Emissions state; reads @c layer_flux_, writes
    ///                        @c tendency_. Legacy states without layer flux
    ///                        use surface_flux_ and injection_layer_.
    /// @param air_density     Air density [kg m-3], one value per atm element.
    /// @param layer_thickness Layer thickness [m], one value per atm element.
    /// @param n_atm_elements  Length of @p air_density and @p layer_thickness.
    ///
    /// @throws MiemException (Validation / CELL_COUNT_MISMATCH) on a size
    ///         mismatch, and (Validation / MASS_CONSERVATION) when
    ///         @c MIEM_CHECK_MASS_CONSERVATION is enabled and the column
    ///         integral drifts beyond @c kMassToleranceFactor.
    static void Apply(EmissionsState& state, const Real* air_density, const Real* layer_thickness, int n_atm_elements);

    /// @brief Per-species, per-cell scalar form of the flux-to-tendency map.
    ///
    /// @param flux_kg_m2_s        Surface flux [kg m-2 s-1].
    /// @param air_density_kg_m3   Air density [kg m-3].
    /// @param layer_thickness_m   Layer thickness [m].
    /// @return Volumetric tendency [kg kg-1 s-1], or zero when the denominator
    ///         is non-positive.
    static Real FluxToTendency(Real flux_kg_m2_s, Real air_density_kg_m3, Real layer_thickness_m);

    /// @brief Maximum allowed relative drift in the mass-conservation check.
    static constexpr Real kMassToleranceFactor = static_cast<Real>(1e-9);
  };

}  // namespace miem
