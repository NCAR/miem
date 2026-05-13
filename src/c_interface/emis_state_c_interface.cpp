// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "error_handling.hpp"
#include "miem/emis_state.hpp"
#include "miem/miem_c.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<::miem::Real, double>,
              "MIEM C API requires Real == double.");

using namespace ::miem;
using namespace ::miem::c_api;

// The opaque `miem_state_t` definition lives here so that this TU and
// `miem_c_interface.cpp` share the same layout — the type must be
// defined identically in both translation units.  Keep this in sync
// with `miem_c_interface.cpp`.
struct miem_state_t
{
  EmisState state_;
};

extern "C" {

double* MIEMGetSurfaceFlux(miem_state_t* state)
{
  if (!state) return nullptr;
  return state->state_.SurfaceFluxData();
}

double* MIEMGetTendency(miem_state_t* state)
{
  if (!state) return nullptr;
  return state->state_.TendencyData();
}

int* MIEMGetEmisToChemIdx(miem_state_t* state)
{
  if (!state) return nullptr;
  return state->state_.EmisToChemIdxData();
}

int MIEMGetStateNumSpecies(const miem_state_t* state)
{
  return state ? state->state_.n_species_ : 0;
}

int MIEMGetStateNumCells(const miem_state_t* state)
{
  return state ? state->state_.n_cells_ : 0;
}

int MIEMGetStateNumVertLevels(const miem_state_t* state)
{
  return state ? state->state_.n_vert_levels_ : 0;
}

void DeleteMIEMState(miem_state_t* state)
{
  delete state;
}

int MIEMGetSectorCount(const miem_state_t* state)
{
  if (!state) return 0;
  return static_cast<int>(state->state_.sector_names_.size());
}

int MIEMGetSectorName(const miem_state_t* state, int i, char* out,
                      MIEM_Error* err)
{
  ClearError(err);
  if (!state || !out)
  {
    SetError(err, 1, "ConfigInvalid", "MIEMGetSectorName: null argument");
    return 1;
  }
  if (i < 0 || i >= static_cast<int>(state->state_.sector_names_.size()))
  {
    SetError(err, 1, "UnknownSector", "MIEMGetSectorName: index out of range");
    return 1;
  }
  std::strncpy(out, state->state_.sector_names_[i].c_str(),
               MIEM_MAX_NAME_LEN - 1);
  out[MIEM_MAX_NAME_LEN - 1] = '\0';
  return 0;
}

double* MIEMGetSectorFlux(miem_state_t* state, const char* sector_name,
                          MIEM_Error* err)
{
  ClearError(err);
  if (!state || !sector_name)
  {
    SetError(err, 1, "ConfigInvalid", "MIEMGetSectorFlux: null argument");
    return nullptr;
  }
  auto it = state->state_.sector_fluxes_.find(sector_name);
  if (it == state->state_.sector_fluxes_.end())
  {
    SetError(err, 1, "UnknownSector",
             (std::string("MIEMGetSectorFlux: no such sector '") +
              sector_name + "'").c_str());
    return nullptr;
  }
  return it->second.data();
}

}  // extern "C"
