#include "error_handling.hpp"
#include "miem/emis_state.hpp"

using namespace miem;
using namespace miem::c_api;

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

void DeleteMIEMState(void* state, MIEMError* error) {
  HandleErrors(error, [&]() {
    delete static_cast<EmisState*>(state);
  });
}

}  // extern "C"
