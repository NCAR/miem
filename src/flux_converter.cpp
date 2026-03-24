#include "miem/flux_converter.hpp"

#include <cmath>

#include "miem/util/error.hpp"

namespace miem {

Real FluxConverter::FluxToTendency(Real flux_kg_m2_s,
                                   Real air_density_kg_m3,
                                   Real layer_thickness_m) {
  if (air_density_kg_m3 <= 0.0 || layer_thickness_m <= 0.0) {
    return 0.0;
  }
  return flux_kg_m2_s / (air_density_kg_m3 * layer_thickness_m);
}

void FluxConverter::Convert(EmisState& state,
                            const std::vector<Real>& air_density,
                            const std::vector<Real>& layer_thickness) {
  const int n_sp = state.n_species;
  const int n_cells = state.n_cells;
  const int n_vl = state.n_vert_levels;

  if (static_cast<int>(air_density.size()) < n_vl * n_cells ||
      static_cast<int>(layer_thickness.size()) < n_vl * n_cells) {
    throw ValidationError(
        "FluxConverter: air_density and layer_thickness must have at least "
        "n_vert_levels * n_cells elements");
  }

  // Zero tendency before accumulating
  std::fill(state.tendency.begin(), state.tendency.end(), 0.0);

  for (int isp = 0; isp < n_sp; ++isp) {
    int layer = state.injection_layer[isp];
    if (layer < 0 || layer >= n_vl) {
      layer = 0;  // Default to surface
    }

    for (int ic = 0; ic < n_cells; ++ic) {
      Real flux = state.surface_flux[isp * n_cells + ic];
      Real rho = air_density[layer * n_cells + ic];
      Real dz = layer_thickness[layer * n_cells + ic];

      // tendency index: [species * n_vert_levels * n_cells + level * n_cells + cell]
      size_t tend_idx =
          static_cast<size_t>(isp) * n_vl * n_cells + layer * n_cells + ic;
      state.tendency[tend_idx] = FluxToTendency(flux, rho, dz);
    }
  }
}

}  // namespace miem
