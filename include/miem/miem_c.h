/*
 * MIEM — Model Independent Emissions Module
 * Public C API header
 *
 * All functions in this header use double precision regardless of the
 * internal MIEM_USE_DOUBLE compile flag.
 */

#ifndef MIEM_C_H
#define MIEM_C_H

#ifdef __cplusplus
extern "C" {
#endif

#define MIEM_MAX_SPECIES_NAME_LEN 64

/* Error struct returned by all fallible functions */
typedef struct {
  int code;                              /* 0 = success */
  char category[MIEM_MAX_SPECIES_NAME_LEN];
  char message[256];
} MIEM_Error;

/* --- Lifecycle ----------------------------------------------------------- */

/* Create an MIEM instance from a YAML config file.
 * Returns an opaque handle; caller must call DeleteMIEM when done. */
void* CreateMIEM(const char* config_path, int n_cells, int n_vert_levels,
                 MIEM_Error* error);

/* Destroy an MIEM instance. */
void DeleteMIEM(void* miem, MIEM_Error* error);

/* --- Species discovery --------------------------------------------------- */

/* Query the number of mechanism species this config will produce. */
int MIEMQuerySpeciesCount(const char* config_path, MIEM_Error* error);

/* Query mechanism species names. Caller allocates names[max_names],
 * each pointing to a buffer of at least MIEM_MAX_SPECIES_NAME_LEN bytes. */
void MIEMQuerySpeciesNames(const char* config_path, char** names,
                           int max_names, MIEM_Error* error);

/* Map emission species to host model chemistry indices.
 * host_names: array of n_host C strings
 * indices: output array of size >= NumSpecies(miem) */
void MIEMResolveHostIndices(void* miem, const char** host_names, int n_host,
                            int* indices, MIEM_Error* error);

/* --- Run ----------------------------------------------------------------- */

/* Execute one emissions time step. Returns a heap-allocated EmisState;
 * caller must call DeleteMIEMState when done.
 * time: seconds since epoch
 * air_density, layer_thickness: (n_vert_levels * n_cells) arrays in
 *   kg/m^3 and meters respectively. */
void* MIEMRun(void* miem, double time,
              const double* air_density, const double* layer_thickness,
              int n_atm_elements, MIEM_Error* error);

/* --- EmisState accessors ------------------------------------------------- */

/* Get pointer to surface flux array: (n_species * n_cells) in kg/m^2/s
 * Layout: flux[species_idx * n_cells + cell_idx] */
double* MIEMGetSurfaceFlux(void* state);

/* Get pointer to tendency array: (n_species * n_vert_levels * n_cells)
 * in kg/kg/s.
 * Layout: tend[species * n_vert_levels * n_cells + level * n_cells + cell] */
double* MIEMGetTendency(void* state);

/* Get pointer to emission-to-chemistry index map.
 * indices[i] = host chemistry index for emission species i, or -1 */
int* MIEMGetEmisToChemIdx(void* state);

int MIEMGetStateNumSpecies(void* state);
int MIEMGetStateNumCells(void* state);
int MIEMGetStateNumVertLevels(void* state);
int MIEMGetNumSpecies(void* miem);

/* Free an EmisState returned by MIEMRun. */
void DeleteMIEMState(void* state, MIEM_Error* error);

/* --- Sector accessors ---------------------------------------------------- */

/* Get the number of sectors in the emission state. */
int MIEMGetSectorCount(void* state);

/* Get sector names. Caller allocates names[max_names], each pointing to a
 * buffer of at least MIEM_MAX_SPECIES_NAME_LEN bytes. */
void MIEMGetSectorNames(void* state, char** names, int max_names,
                         MIEM_Error* error);

/* Get pointer to the flux array for a given sector name.
 * Returns (n_species * n_cells) in kg/m^2/s, or NULL if not found. */
double* MIEMGetSectorFlux(void* state, const char* sector_name,
                           MIEM_Error* error);

#ifdef __cplusplus
}
#endif

#endif /* MIEM_C_H */
