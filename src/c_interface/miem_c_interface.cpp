#include "error_handling.hpp"
#include "miem/emissions_module.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace miem;
using namespace miem::c_api;

extern "C" {

void* CreateMIEM(const char* config_path, int n_cells, int n_vert_levels,
                 MIEMError* error) {
  EmissionsModule* module = nullptr;
  HandleErrors(error, [&]() {
    module = new EmissionsModule(config_path, n_cells, n_vert_levels);
  });
  return static_cast<void*>(module);
}

void DeleteMIEM(void* miem, MIEMError* error) {
  HandleErrors(error, [&]() {
    delete static_cast<EmissionsModule*>(miem);
  });
}

int MIEMQuerySpeciesCount(const char* config_path, MIEMError* error) {
  int count = 0;
  HandleErrors(error, [&]() {
    auto species = EmissionsModule::QuerySpecies(config_path);
    count = static_cast<int>(species.size());
  });
  return count;
}

void MIEMQuerySpeciesNames(const char* config_path, char** names,
                           int max_names, MIEMError* error) {
  HandleErrors(error, [&]() {
    auto species = EmissionsModule::QuerySpecies(config_path);
    int n = std::min(static_cast<int>(species.size()), max_names);
    for (int i = 0; i < n; ++i) {
      std::strncpy(names[i], species[i].c_str(), 63);
      names[i][63] = '\0';
    }
  });
}

void MIEMResolveHostIndices(void* miem, const char** host_names, int n_host,
                            int* indices, MIEMError* error) {
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

void* MIEMRun(void* miem, double time, double dt,
              const double* air_density, const double* layer_thickness,
              int n_atm_elements, MIEMError* error) {
  EmisState* state = nullptr;
  HandleErrors(error, [&]() {
    auto* module = static_cast<EmissionsModule*>(miem);
    std::vector<Real> rho(air_density, air_density + n_atm_elements);
    std::vector<Real> dz(layer_thickness, layer_thickness + n_atm_elements);
    state = new EmisState(module->Run(time, dt, rho, dz));
  });
  return static_cast<void*>(state);
}

int MIEMGetNumSpecies(void* miem) {
  if (!miem) return 0;
  return static_cast<EmissionsModule*>(miem)->NumSpecies();
}

}  // extern "C"
