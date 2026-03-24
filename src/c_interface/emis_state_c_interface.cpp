#include "error_handling.hpp"
#include "miem/emis_state.hpp"

#include <algorithm>
#include <cstring>
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

int MIEMGetSectorCount(void* state) {
  if (!state) return 0;
  return static_cast<int>(
      static_cast<EmisState*>(state)->sector_names.size());
}

void MIEMGetSectorNames(void* state, char** names, int max_names,
                         MIEM_Error* error) {
  HandleErrors(error, [&]() {
    auto* s = static_cast<EmisState*>(state);
    int n = std::min(static_cast<int>(s->sector_names.size()), max_names);
    for (int i = 0; i < n; ++i) {
      std::strncpy(names[i], s->sector_names[i].c_str(),
                    MIEM_MAX_SPECIES_NAME_LEN - 1);
      names[i][MIEM_MAX_SPECIES_NAME_LEN - 1] = '\0';
    }
  });
}

double* MIEMGetSectorFlux(void* state, const char* sector_name,
                           MIEM_Error* error) {
  double* result = nullptr;
  HandleErrors(error, [&]() {
    auto* s = static_cast<EmisState*>(state);
    auto it = s->sector_fluxes.find(sector_name);
    if (it == s->sector_fluxes.end()) {
      throw IOError(std::string("No sector flux for: ") + sector_name);
    }
    result = it->second.data();
  });
  return result;
}

}  // extern "C"
