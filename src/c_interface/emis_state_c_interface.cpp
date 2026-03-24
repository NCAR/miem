#include "error_handling.hpp"
#include "miem/emis_state.hpp"

#include <type_traits>

using namespace miem;
using namespace miem::c_api;

// The C API always uses double. If Real != double, the pointer casts below
// would silently corrupt data. Fail at compile time instead.
static_assert(std::is_same_v<Real, double>,
    "MIEM C API requires double precision. Build with MIEM_DOUBLE_PRECISION=ON.");

extern "C" {

double* MIEMGetSurfaceFlux(void* state) {
  if (!state) return nullptr;
  return static_cast<EmisState*>(state)->SurfaceFluxData();
}

double* MIEMGetTendency(void* state) {
  if (!state) return nullptr;
  return static_cast<EmisState*>(state)->TendencyData();
}

int* MIEMGetEmisToChemIdx(void* state) {
  if (!state) return nullptr;
  return static_cast<EmisState*>(state)->EmisToChemIdxData();
}

int MIEMGetStateNumSpecies(void* state) {
  if (!state) return 0;
  return static_cast<EmisState*>(state)->n_species;
}

int MIEMGetStateNumCells(void* state) {
  if (!state) return 0;
  return static_cast<EmisState*>(state)->n_cells;
}

int MIEMGetStateNumVertLevels(void* state) {
  if (!state) return 0;
  return static_cast<EmisState*>(state)->n_vert_levels;
}

void DeleteMIEMState(void* state, MIEM_Error* error) {
  HandleErrors(error, [&]() {
    delete static_cast<EmisState*>(state);
  });
}

}  // extern "C"
