#include "error_handling.hpp"
#include "miem/emissions_module.hpp"
#include "miem/miem_c.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace miem;
using namespace miem::c_api;

static_assert(std::is_same_v<Real, double>,
    "MIEM C API requires double precision. Build with MIEM_DOUBLE_PRECISION=ON.");

extern "C" {

void* CreateMIEM(const char* config_path, int n_cells, int n_vert_levels,
                 MIEM_Error* error) {
  EmissionsModule* module = nullptr;
  HandleErrors(error, [&]() {
    module = new EmissionsModule(config_path, n_cells, n_vert_levels);
  });
  return static_cast<void*>(module);
}

void DeleteMIEM(void* miem, MIEM_Error* error) {
  HandleErrors(error, [&]() {
    delete static_cast<EmissionsModule*>(miem);
  });
}

int MIEMQuerySpeciesCount(const char* config_path, MIEM_Error* error) {
  int count = 0;
  HandleErrors(error, [&]() {
    auto species = EmissionsModule::QuerySpecies(config_path);
    count = static_cast<int>(species.size());
  });
  return count;
}

void MIEMQuerySpeciesNames(const char* config_path, char** names,
                           int max_names, MIEM_Error* error) {
  HandleErrors(error, [&]() {
    auto species = EmissionsModule::QuerySpecies(config_path);
    int n = std::min(static_cast<int>(species.size()), max_names);
    for (int i = 0; i < n; ++i) {
      std::strncpy(names[i], species[i].c_str(), MIEM_MAX_SPECIES_NAME_LEN - 1);
      names[i][MIEM_MAX_SPECIES_NAME_LEN - 1] = '\0';
    }
  });
}

void MIEMResolveHostIndices(void* miem, const char** host_names, int n_host,
                            int* indices, MIEM_Error* error) {
  HandleErrors(error, [&]() {
    auto* module = static_cast<EmissionsModule*>(miem);
    std::vector<std::string> host_species(n_host);
    for (int i = 0; i < n_host; ++i) {
      host_species[i] = host_names[i];
    }
    std::vector<int> idx;
    module->ResolveHostIndices(host_species, idx);
    for (int i = 0; i < static_cast<int>(idx.size()); ++i) {
      indices[i] = idx[i];
    }
  });
}

void* MIEMRun(void* miem, double time,
              const double* air_density, const double* layer_thickness,
              int n_atm_elements, MIEM_Error* error) {
  EmisState* state = nullptr;
  HandleErrors(error, [&]() {
    auto* module = static_cast<EmissionsModule*>(miem);
    state = new EmisState(module->Run(time, air_density, layer_thickness,
                                      n_atm_elements));
  });
  return static_cast<void*>(state);
}

int MIEMGetNumSpecies(void* miem) {
  if (!miem) return 0;
  return static_cast<EmissionsModule*>(miem)->NumSpecies();
}

}  // extern "C"
