// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// C API implementation.  Opaque structs `miem_config_t` / `miem_t` /
// `miem_state_t` wrap the matching C++ types; the boundary translates
// `Result<T>` and exceptions into `MIEM_Error`.

#include "error_handling.hpp"
#include "miem/config.hpp"
#include "miem/emissions_module.hpp"
#include "miem/miem_c.h"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// C API is unconditionally double — fail at compile time if not.
static_assert(std::is_same_v<::miem::Real, double>,
              "MIEM C API requires Real == double.  Build with "
              "MIEM_USE_DOUBLE=ON or omit miem_c from the build.");

using namespace ::miem;
using namespace ::miem::c_api;

/* -------------------------------------------------------------------- */
/* Opaque wrappers.                                                     */

struct miem_config_t
{
  MIEMConfig cfg_;
};

struct miem_t
{
  std::unique_ptr<EmissionsModule> module_;
};

struct miem_state_t
{
  EmisState state_;
};

namespace {

SourceMode AsMode(int v)
{
  return v == 1 ? SourceMode::Online : SourceMode::Offline;
}

SourceType AsType(int v)
{
  switch (v)
  {
    case MIEM_SOURCE_TYPE_FIRE:         return SourceType::Fire;
    case MIEM_SOURCE_TYPE_BIOGENIC:     return SourceType::Biogenic;
    case MIEM_SOURCE_TYPE_DUST:         return SourceType::Dust;
    case MIEM_SOURCE_TYPE_SEA_SALT:     return SourceType::SeaSalt;
    case MIEM_SOURCE_TYPE_LIGHTNING:    return SourceType::Lightning;
    case MIEM_SOURCE_TYPE_ANTHROPOGENIC:
    default:                            return SourceType::Anthropogenic;
  }
}

TemporalInterpolation AsTemporal(int v)
{
  switch (v)
  {
    case 1:  return TemporalInterpolation::Nearest;
    case 2:  return TemporalInterpolation::None;
    case 0:
    default: return TemporalInterpolation::Linear;
  }
}

VerticalInjection AsVertical(int v)
{
  return v == 1 ? VerticalInjection::Plume : VerticalInjection::Surface;
}

}  // namespace

extern "C" {

/* -------------------------------------------------------------------- */
/* Config building                                                      */

miem_config_t* miem_config_new(void)
{
  return new (std::nothrow) miem_config_t{};
}

void miem_config_delete(miem_config_t* cfg)
{
  delete cfg;
}

void miem_config_set_version(miem_config_t* cfg, const char* version)
{
  if (!cfg || !version) return;
  cfg->cfg_.version_ = version;
}

void miem_config_set_regridding_none(miem_config_t* cfg)
{
  if (!cfg) return;
  cfg->cfg_.regridding_.type_ = RegriddingType::None;
  cfg->cfg_.regridding_.weights_file_.clear();
}

int miem_config_add_source(miem_config_t*            cfg,
                           const miem_source_spec_t* spec,
                           MIEM_Error*               err)
{
  ClearError(err);
  if (!cfg || !spec || !spec->name)
  {
    SetError(err, 1, "ConfigInvalid",
             "miem_config_add_source: null cfg/spec/name");
    return 1;
  }

  SourceConfig sc;
  sc.name_                   = spec->name;
  sc.mode_                   = AsMode(spec->mode);
  sc.type_                   = AsType(spec->type);
  sc.file_pattern_           = spec->file_pattern ? spec->file_pattern : "";
  sc.convention_             = spec->convention   ? spec->convention   : "eccad";
  sc.temporal_interpolation_ = AsTemporal(spec->temporal_interpolation);
  sc.vertical_injection_     = AsVertical(spec->vertical_injection);
  sc.category_               = spec->category;
  sc.hierarchy_              = spec->hierarchy;
  sc.scaling_factor_         = static_cast<Real>(spec->scaling_factor);
  sc.sector_                 = spec->sector ? spec->sector : "";

  cfg->cfg_.sources_.push_back(std::move(sc));
  return 0;
}

int miem_config_add_species_mapping(miem_config_t* cfg,
                                    const char*    source_name,
                                    const char*    inventory_species,
                                    const char*    mechanism_species,
                                    double         scaling_factor,
                                    MIEM_Error*    err)
{
  ClearError(err);
  if (!cfg || !source_name || !inventory_species || !mechanism_species)
  {
    SetError(err, 1, "ConfigInvalid",
             "miem_config_add_species_mapping: null argument");
    return 1;
  }

  for (auto& src : cfg->cfg_.sources_)
  {
    if (src.name_ == source_name)
    {
      src.species_map_.AddMapping(inventory_species, mechanism_species,
                                  static_cast<Real>(scaling_factor));
      return 0;
    }
  }

  SetError(err, 1, "ConfigInvalid",
           (std::string("source '") + source_name +
            "' not found; call miem_config_add_source first").c_str());
  return 1;
}

int miem_config_validate(const miem_config_t* cfg, MIEM_Error* err)
{
  ClearError(err);
  if (!cfg)
  {
    SetError(err, 1, "ConfigInvalid", "miem_config_validate: null cfg");
    return 1;
  }
  auto r = cfg->cfg_.Validate();
  if (!r)
  {
    SetErrorFromEntry(err, r.errors().front());
    return 1;
  }
  return 0;
}

/* -------------------------------------------------------------------- */
/* Lifecycle                                                            */

int CreateMIEM(const miem_config_t* cfg,
               int                  n_cells,
               int                  n_vert_levels,
               miem_t**             handle,
               MIEM_Error*          err)
{
  ClearError(err);
  if (!cfg || !handle)
  {
    SetError(err, 1, "ConfigInvalid", "CreateMIEM: null cfg or handle");
    return 1;
  }

  int rc = 1;
  HandleErrors(err, [&]() {
    auto created = EmissionsModule::Create(cfg->cfg_, n_cells, n_vert_levels);
    if (!created)
    {
      SetErrorFromEntry(err, created.errors().front());
      *handle = nullptr;
      return;
    }
    auto* wrapper      = new miem_t{};
    wrapper->module_   = std::move(created).value();
    *handle            = wrapper;
    rc                 = 0;
  });
  return rc;
}

void DeleteMIEM(miem_t* handle)
{
  delete handle;
}

int MIEMGetNumSpecies(const miem_t* handle)
{
  if (!handle || !handle->module_) return 0;
  return handle->module_->NumSpecies();
}

int MIEMResolveHostIndices(miem_t*       handle,
                           const char**  host_names,
                           int           n_host,
                           int*          indices,
                           MIEM_Error*   err)
{
  ClearError(err);
  if (!handle || !handle->module_ || !host_names || !indices)
  {
    SetError(err, 1, "ConfigInvalid", "MIEMResolveHostIndices: null argument");
    return 1;
  }
  std::vector<std::string> host_species;
  host_species.reserve(static_cast<std::size_t>(n_host));
  for (int i = 0; i < n_host; ++i)
  {
    host_species.emplace_back(host_names[i] ? host_names[i] : "");
  }
  std::vector<int> idx;
  handle->module_->ResolveHostIndices(host_species, idx);
  for (std::size_t i = 0; i < idx.size(); ++i)
  {
    indices[i] = idx[i];
  }
  return 0;
}

/* -------------------------------------------------------------------- */
/* Run                                                                  */

int MIEMRun(miem_t*        handle,
            double         time_sec,
            double         dt_sec,
            const double*  air_density,
            const double*  layer_thickness,
            int            n_atm_elements,
            miem_state_t** state_out,
            MIEM_Error*    err)
{
  ClearError(err);
  if (!handle || !handle->module_ || !state_out)
  {
    SetError(err, 1, "ConfigInvalid", "MIEMRun: null argument");
    return 1;
  }

  Result<EmisState> r = (air_density && layer_thickness && n_atm_elements > 0)
                           ? handle->module_->Run(time_sec, dt_sec,
                                                  air_density, layer_thickness,
                                                  n_atm_elements)
                           : handle->module_->Run(time_sec, dt_sec);

  if (!r)
  {
    SetErrorFromEntry(err, r.errors().front());
    *state_out = nullptr;
    return 1;
  }

  auto* wrapper    = new miem_state_t{};
  wrapper->state_  = std::move(r).value();
  *state_out       = wrapper;
  return 0;
}

}  // extern "C"
