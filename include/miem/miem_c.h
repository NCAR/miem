/*
 * Copyright (C) 2026 National Center for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 *
 * MIEM — Model Independent Emissions Module
 * Public C API header.
 *
 * Hosts and musica build a config programmatically via the setters, then
 * call `CreateMIEM` with the populated handle.  No path-taking entry
 * point exists — MIEM does not parse YAML at any level.
 *
 * All functions in this header use double precision regardless of the
 * internal MIEM_USE_DOUBLE flag (the C-API translation unit asserts
 * MIEM_USE_DOUBLE at compile time).
 */

#ifndef MIEM_C_H
#define MIEM_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIEM_MAX_NAME_LEN     64
#define MIEM_MAX_MESSAGE_LEN  256
#define MIEM_MAX_CATEGORY_LEN 64

/* ------------------------------------------------------------------------ */
/* Error reporting                                                           */
/* ------------------------------------------------------------------------ */

typedef struct {
  int  code;                                   /* 0 = success           */
  char category[MIEM_MAX_CATEGORY_LEN];        /* category enum tag     */
  char message[MIEM_MAX_MESSAGE_LEN];          /* human-readable detail */
} MIEM_Error;

/* ------------------------------------------------------------------------ */
/* Opaque handles                                                            */
/* ------------------------------------------------------------------------ */

typedef struct miem_config_t miem_config_t;
typedef struct miem_t        miem_t;
typedef struct miem_state_t  miem_state_t;

/* Public POD for adding sources.  Strings are NUL-terminated and copied
 * by `miem_config_add_source`.  Field order matches MIEMConfig's
 * trailing-underscore fields in C++. */
typedef struct {
  const char* name;                    /* required */
  int         mode;                    /* 0 = offline, 1 = online (rejected) */
  int         type;                    /* SourceType enum, see below         */
  const char* file_pattern;            /* required */
  const char* convention;              /* "eccad" only in v1                 */
  int         temporal_interpolation;  /* 0=linear, 1=nearest, 2=none        */
  int         vertical_injection;      /* 0=surface, 1=plume (rejected)      */
  int         category;
  int         hierarchy;
  double      scaling_factor;
  const char* sector;                  /* may be NULL or "" */
} miem_source_spec_t;

/* SourceType enum values for `miem_source_spec_t::type`. */
#define MIEM_SOURCE_TYPE_ANTHROPOGENIC 0
#define MIEM_SOURCE_TYPE_FIRE          1
#define MIEM_SOURCE_TYPE_BIOGENIC      2
#define MIEM_SOURCE_TYPE_DUST          3
#define MIEM_SOURCE_TYPE_SEA_SALT      4
#define MIEM_SOURCE_TYPE_LIGHTNING     5

/* ------------------------------------------------------------------------ */
/* Config building                                                           */
/* ------------------------------------------------------------------------ */

miem_config_t* miem_config_new(void);
void           miem_config_delete(miem_config_t* cfg);

void miem_config_set_version(miem_config_t* cfg, const char* version);
void miem_config_set_regridding_none(miem_config_t* cfg);

/* Append a source.  Returns 0 on success, nonzero on failure (and
 * populates `err` if non-NULL). */
int  miem_config_add_source(miem_config_t*            cfg,
                            const miem_source_spec_t* spec,
                            MIEM_Error*               err);

/* Add a species-map entry to the most recently added source (matched by
 * `source_name`).  Returns 0 on success. */
int  miem_config_add_species_mapping(miem_config_t* cfg,
                                     const char*    source_name,
                                     const char*    inventory_species,
                                     const char*    mechanism_species,
                                     double         scaling_factor,
                                     MIEM_Error*    err);

/* Validate the populated config.  Returns 0 on success. */
int  miem_config_validate(const miem_config_t* cfg, MIEM_Error* err);

/* ------------------------------------------------------------------------ */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------ */

/* Build an MIEM instance from the populated config.  Returns 0 on
 * success and writes the handle to `*handle`.  `cfg` is consumed by
 * copy; the caller still owns the `miem_config_t`. */
int  CreateMIEM(const miem_config_t* cfg,
                int                  n_cells,
                int                  n_vert_levels,
                miem_t**             handle,
                MIEM_Error*          err);

void DeleteMIEM(miem_t* handle);

int  MIEMGetNumSpecies(const miem_t* handle);

/* Resolve module species to host indices, -1 when the host does not
 * provide a given mechanism species. */
int  MIEMResolveHostIndices(miem_t*       handle,
                            const char**  host_names,
                            int           n_host,
                            int*          indices,
                            MIEM_Error*   err);

/* ------------------------------------------------------------------------ */
/* Run                                                                       */
/* ------------------------------------------------------------------------ */

/* Execute one emissions time step.  Returns 0 on success and writes the
 * state to `*state_out` (heap-allocated; caller frees via
 * DeleteMIEMState).
 *
 * `air_density` and `layer_thickness` are (n_vert_levels * n_cells)
 * flat arrays in kg/m³ and m respectively; layout
 * [level * n_cells + cell].  Pass NULL for both (and n_atm_elements=0)
 * to skip tendency conversion. */
int  MIEMRun(miem_t*        handle,
             double         time_sec,
             double         dt_sec,
             const double*  air_density,
             const double*  layer_thickness,
             int            n_atm_elements,
             miem_state_t** state_out,
             MIEM_Error*    err);

void DeleteMIEMState(miem_state_t* state);

/* ------------------------------------------------------------------------ */
/* EmisState accessors (zero-copy)                                           */
/* ------------------------------------------------------------------------ */

/* (n_species * n_cells) kg/m²/s, layout [species * n_cells + cell] */
double* MIEMGetSurfaceFlux(miem_state_t* state);

/* (n_species * n_vert_levels * n_cells) kg/kg/s */
double* MIEMGetTendency(miem_state_t* state);

int*    MIEMGetEmisToChemIdx(miem_state_t* state);

int     MIEMGetStateNumSpecies(const miem_state_t* state);
int     MIEMGetStateNumCells(const miem_state_t* state);
int     MIEMGetStateNumVertLevels(const miem_state_t* state);

/* Sector diagnostic accessors */
int     MIEMGetSectorCount(const miem_state_t* state);

/* Copy sector name `i` into `out` (at least MIEM_MAX_NAME_LEN bytes).
 * Returns 0 on success. */
int     MIEMGetSectorName(const miem_state_t* state, int i,
                          char* out, MIEM_Error* err);

/* Returns a pointer to the (n_species * n_cells) sector flux array.
 * Returns NULL on lookup failure (and populates `err`). */
double* MIEMGetSectorFlux(miem_state_t* state, const char* sector_name,
                          MIEM_Error* err);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MIEM_C_H */
