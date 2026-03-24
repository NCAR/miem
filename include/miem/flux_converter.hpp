#pragma once

#include "miem/emis_state.hpp"
#include "miem/util/types.hpp"

namespace miem {

class FluxConverter {
 public:
  // Convert surface fluxes in EmisState to volumetric tendencies
  // using host-provided atmospheric state.
  //
  // air_density: (n_vert_levels * n_cells) in kg/m^3
  // layer_thickness: (n_vert_levels * n_cells) in meters
  // n_atm_elements: size of air_density and layer_thickness arrays
  //
  // Both arrays use layout: [level * n_cells + cell]
  static void Convert(EmisState& state,
                      const Real* air_density,
                      const Real* layer_thickness,
                      int n_atm_elements);

  // Convert a single species flux to tendency at a given injection layer
  static Real FluxToTendency(Real flux_kg_m2_s,
                             Real air_density_kg_m3,
                             Real layer_thickness_m);
};

}  // namespace miem
