# MIEM Scaffolding Port — Synthesized Implementation Plan

## Executive summary

This plan ports the runtime kernels from `feature/scaffolding` onto `feature/scaffolding-port`, conforming the result to the canonical architecture in `docs/config-architecture.md`. MIEM becomes "the MICM of emissions": no `yaml-cpp`, no `MechanismConfiguration` dependency, no path-taking entry points. The public surface is a struct-based C++ API plus a setters-and-handle C API; musica is the only YAML-aware caller.

**Ported in:** `EmisState`, `EmissionsModule`, `SpeciesMap` (programmatic-only), `TemporalInterpolator`, `FluxConverter`, `OfflineEmissionSource`, `SourceFactory`, `SESReader` (renamed `ECCADReader` in-place where the call site is local; public type alias retained). **Dropped:** every `FromYAML` method, `yaml-cpp` linkage at any level, the path-taking `CreateMIEM` C entry, Fortran bindings, regridding code paths (struct field remains but is asserted `kNone`), climatological wrap-around in `source_offline.cpp` (replaced with a hard error). **Deferred to `src/scope-uncertain/`:** `DatasetDescriptor`, `dataset_descriptor.cpp`, and `descriptor`-related branches in `OfflineEmissionSource` (gated by config — non-ECCAD `convention` is rejected by `MIEMConfig` validation, never reaches the descriptor code).

**Key engineering decisions:** `Result<T>` modeled on `mechanism_configuration::ParserResult<T>` at every public boundary; `Real` typedef preserved with `MIEM_USE_DOUBLE=ON` by default and a double-only mirror at the C API; three CMake targets (`miem_objects` OBJECT, `miem` STATIC, `miem_c` STATIC linking the same objects); full `find_package(miem)` config-package install; trailing-underscore field naming retained from PR #8.

**Effort:** ~5–7 engineer-days for an implementer following this spec end-to-end. Net add: ~1500 LOC source, ~600 LOC CMake/install, no test code (separate agent).

## Resolved decisions (was contested in planning)

### D1. Error handling: `Result<T>` at public boundary, exceptions internal-only

**Decision:** Public C++ API returns `miem::Result<T>` modeled on `mechanism_configuration::ParserResult<T>` — `operator bool`, `errors` vector of `{ErrorCode, std::string}`, optional `value`. Constructors that cannot meaningfully fail (e.g., `EmissionsModule(MIEMConfig, n_cells, n_levels)`) instead take a pre-validated config and are noexcept; the validation step is a separate `MIEMConfig::Validate() -> Result<void>` that the caller invokes (or that `Create` factory functions call internally). The C API uses an `MIEM_Error` out-parameter populated from `Result` via an `unwrap_or_set` template helper at the boundary.

**Internal kernels may throw** `miem::MIEMError` and its derivatives (`ConfigError`, `IOError`, `ValidationError`, `SpeciesError`) — those exist in scaffolding and are useful for short-circuiting deep call stacks (e.g., inside `SESReader::Open`). A boundary-layer `catch (const MIEMError&)` at every public `Result`-returning function and every C-API entry converts them to `Result::Error{code, message}`.

**Why this side:** Architecture doc §9 explicitly says "No exceptions at the public API" and uses `ParserResult<T>` as the model. planner-engineer's position aligns with the canonical doc; planner-musica's pure-exception position contradicts it. The hybrid (internal throw, boundary catch) is a small concession that lets kernels stay readable without leaking exceptions into musica or host code.

### D2. Public-struct field naming: keep trailing underscores

**Decision:** Preserve PR #8's trailing-underscore convention on public POD structs (`MIEMConfig::sources_`, `SourceConfig::name_`, `EmisState::surface_flux_`, etc.). Do *not* rename to bare `name`, `sources`, `surface_flux` even though MICM uses bare names.

**Why this side:** Renaming churns every header, every include site in musica's eventual translator, and PR #8 has already shipped the trailing-underscore choice on `feature/scaffolding-port`. The cosmetic-alignment cost is paid once; the rename cost is paid every time someone reads a diff. planner-musica's "MICM-sibling visual identity" argument is real but second-order; planner-engineer's "preserve what shipped" is decisive. *Sidecar:* document the deliberate divergence in a one-line comment at the top of `include/miem/config.hpp` so future readers don't think it's a mistake.

### D3. DatasetDescriptor: scope-uncertain

**Decision:** Move `include/miem/dataset_descriptor.hpp`, `src/dataset_descriptor.cpp`, and any descriptor-touching branches in `OfflineEmissionSource` to `src/scope-uncertain/` with a README explaining the v1 ECCAD-only scope. `MIEMConfig::Validate()` rejects any `Inventory.convention` other than `"eccad"` with `UnknownConvention` before the descriptor code can be reached. The struct definition does *not* appear in `include/miem/` for v1.

**Why this side:** 2-vs-1 planner majority, and the structural argument is correct: ECCAD-only v1 means `DatasetDescriptor` is dead code at runtime. Keeping it in public headers invites accidental use; quarantining it makes the resurrection (when v1.1 lands `convention: descriptor`) explicit and traceable. planner-musica's "keep in header, assert at runtime" loses because runtime assertion is weaker than compile-time absence.

### D4. CMake target structure: 3 targets

**Decision:** Three targets — `miem_objects` (OBJECT library, all `.cpp` core sources), `miem` (STATIC library, links `miem_objects`, public consumer face, aliased as `musica::miem`), `miem_c` (STATIC library, links `miem_objects` plus C-interface sources, aliased as `musica::miem_c`). The OBJECT library lets the C and C++ surfaces share compilation without double-building. Drop the current `add_library(miem INTERFACE)` — MIEM is not header-only.

**Why this side:** planner-engineer's structural argument is sound; planner-musica's 2-target proposal collapses the C and C++ artifacts. The cost of the 3rd target is ~20 lines of CMake. The 3-target split is a one-way door (INTERFACE → STATIC change is breaking already; adding the OBJECT split now costs nothing extra).

### D5. Climatological wrap-around in `source_offline.cpp`: kill it

**Decision:** Remove the `right_idx = 0; left_idx = n_times - 1` branches from `LoadBrackets`. When `time_current` falls outside `[times[0], times[n_times-1]]`, the function returns `Result::Error{TimeOutOfRange, "..."}`. A future v1.1 may add an explicit `SourceConfig::climatology_mode = {kNone, kAnnualWrap}` field; until then, silent extrapolation by fabrication is a science correctness risk.

**Why this side:** planner-scientist's domain authority on physics defaults. The current behavior — bracket between `times[n_times-1]` and `times[0]` with a fabricated `time_right = times[n_times-1] + avg_step` — is silent data invention. A host model running outside the file's time range should fail loudly, not silently produce wrong-but-plausible numbers.

### D6. `Regridding::type = kNone` validated at construction

**Decision:** `MIEMConfig::Validate()` rejects any `Regridding.type != kNone` with `UnsupportedRegriddingType`. `EmissionsModule`'s constructor (taking an already-validated config) also asserts `cfg.regridding_.type_ == RegriddingType::kNone` with a `MIEM_ASSERT` (release-noop in NDEBUG builds, but tested in `EmissionsModule::Run` on the cold path). Defense in depth: validation catches well-behaved callers; assertion catches programmatic-construction bypassers.

### D7. Apply preconditions: return Result, not throw

**Decision:** `SpeciesMap::Apply` (and equivalents in `TemporalInterpolator`, `FluxConverter`) checks preconditions (e.g., `inventory_flux.size() == inventory_names.size() * n_cells`) and returns `Result<void>::Error{...}` on failure. Cascades from D1. Internal NetCDF reads inside `SESReader/ECCADReader::Open` and `::ReadFlux` may throw `IOError`; the boundary `EmissionsModule::Run` catches and converts. The `SpeciesMap::Validate()` invocation added per planner-scientist's emphasis (A) lives in `MIEMConfig::Validate()` and returns `Result`.

## File-by-file specification

Master table. **Source col:** path on `feature/scaffolding`. **Target col:** path on `feature/scaffolding-port`. **API col:** what changes. **Confidence col:** how sure we are this file belongs in v1.

| Source (scaffolding) | Target (port) | API changes | Confidence |
|---|---|---|---|
| `CMakeLists.txt` | `CMakeLists.txt` | Drop `add_library(miem INTERFACE)`; keep options; add `MIEM_USE_DOUBLE=ON` option; add `MIEM_INSTALL=ON` option; remove yaml-cpp from `dependencies.cmake` | High |
| `cmake/dependencies.cmake` | `cmake/dependencies.cmake` | Remove `find_package(yaml-cpp)` and the FetchContent fallback. Keep `find_package(netCDF)`; if not found, FetchContent the NetCDF-C tag musica pins. Add a `netCDF::netcdf_normalized` IMPORTED INTERFACE target so include dirs and lib paths work across Conda/Homebrew/system installs. | High |
| `src/CMakeLists.txt` | `src/CMakeLists.txt` | Rewrite: declare `miem_objects` OBJECT lib over all core `.cpp`; `miem` STATIC linking `miem_objects` + `netCDF::netcdf_normalized PRIVATE`; `miem_c` STATIC linking `miem_objects` + c-interface sources. Add `musica::miem` and `musica::miem_c` aliases. Public include dirs use `BUILD_INTERFACE`/`INSTALL_INTERFACE` generator expressions. | High |
| (new) `cmake/StaticAnalyzers.cmake` | `cmake/StaticAnalyzers.cmake` | Standard MICM-shaped clang-tidy / cppcheck toggles, off by default. | Med |
| (new) `cmake/install.cmake` or inline | inline in root | Full config-package install: `install(TARGETS miem miem_objects miem_c EXPORT miemTargets …)`, `install(DIRECTORY include/miem TYPE INCLUDE)`, `configure_package_config_file`, `write_basic_package_version_file`, `install(EXPORT miemTargets …)`. | High |
| `include/miem/util/types.hpp` | `include/miem/util/types.hpp` | Keep `Real` typedef gated by `MIEM_USE_DOUBLE` (default ON). Add `Index = std::size_t`. Add a `static_assert(std::is_same_v<Real, double>)` inside the C-interface translation unit only (not in the header) so the C API is unconditionally double-precision. | High |
| `include/miem/util/error.hpp` | `include/miem/util/error.hpp` | Keep `MIEMError` hierarchy (used internally). Add a `Result<T>` template modeled on `mechanism_configuration::ParserResult<T>` — `operator bool`, `errors` vector of `{ErrorCode, std::string}`, optional `value` (use `std::optional` for non-void, specialize for `Result<void>`). Add `enum class ErrorCode` with at minimum: `Ok, ConfigInvalid, UnsupportedRegriddingType, UnknownConvention, OnlineSourcesNotSupported, UnsupportedVerticalInjection, FileNotFound, NetCDFError, SpeciesMapScalingExceedsOne, CellCountMismatch, TimeOutOfRange, MassConservationViolation, InternalError`. | High |
| (new) `include/miem/util/result.hpp` | split out from `error.hpp` if it grows | — | Med |
| `include/miem/config.hpp` (PR #8 version, already on port branch) | same path | Keep PR #8's `SourceConfig` + `MIEMConfig` shape and trailing-underscore field naming. Add `Regridding regridding_` field to `MIEMConfig` (struct with `RegriddingType type_ = kNone` and optional `std::string weights_file_`). Add `SpeciesMap species_map_` field to `SourceConfig` (programmatic, not path). Replace `descriptor` field with a sentinel `std::string convention_ = "eccad"` and quarantine the descriptor type. Add `MIEMConfig::Validate() -> Result<void>`. Drop `FromYAML` (already absent in PR #8). One-line comment at top: `// Public POD config types. Trailing-underscore field convention is deliberate; see plan §D2.` | High |
| `include/miem/dataset_descriptor.hpp` | `src/scope-uncertain/dataset_descriptor.hpp` | Move out of public headers. README in scope-uncertain explains rationale. | High |
| `src/dataset_descriptor.cpp` | `src/scope-uncertain/dataset_descriptor.cpp` | Move. Drop `FromYAML` body (delete the function entirely — it's the YAML entry point). | High |
| `include/miem/emis_state.hpp` (PR #8) | same path | Keep PR #8 shape (`FluxArray`, `EmisState` with `surface_flux_`, `tendency_`, `sector_fluxes_`). Replace stub `FluxArray` with a real type: backing `std::vector<Real>` plus dimensions `n_species_, n_cells_` (and `n_vert_levels_` for the tendency case); accessor `operator()(int cell, const std::string& species)` resolves via species name → index map; raw pointer accessors `data()` + `size()` for C API zero-copy. | High |
| `include/miem/emissions_module.hpp` (PR #8 stub) | same path | Replace stub. Public ctor: `EmissionsModule(const MIEMConfig& cfg, int n_cells, int n_vert_levels)` — preconditions: `cfg` already passed `Validate()`. Public method: `Result<EmisState> Run(double sim_time_sec, double dt_sec)`. Move implementation to `src/emissions_module.cpp` (no longer header-only). | High |
| `src/emissions_module.cpp` | `src/emissions_module.cpp` | Lift from scaffolding. Replace exception sites at public boundary with `Result::Error{...}` via boundary helper. Construct `OfflineEmissionSource` instances via `SourceFactory`. Implement category/hierarchy resolution per HEMCO pattern (cross-category sum; within-category highest-hierarchy wins per cell). Sum into `EmisState.surface_flux_`; populate `tendency_` via `FluxConverter`; populate `sector_fluxes_` per `SourceConfig.sector_`. | High |
| `include/miem/source.hpp` | `include/miem/source.hpp` | Abstract base `EmissionSource` with virtual `Update(time, n_cells, flux_out, species_names_out)` returning `Result<void>` and `QuerySpecies() const -> std::vector<std::string>`. | High |
| `include/miem/source_factory.hpp` + `src/source_factory.cpp` | same paths | Factory function `Result<std::unique_ptr<EmissionSource>> CreateSource(const SourceConfig&)`. Dispatch on `mode_` and (for offline) `convention_`. v1: only `mode_ == Offline` + `convention_ == "eccad"`; everything else → `Result::Error`. | High |
| `include/miem/source_offline.hpp` + `src/source_offline.cpp` | same paths | Lift from scaffolding. **Kill the climatological wrap-around** (D5). Validate cell-count consistency. Replace `descriptor_` field with a sentinel: descriptor logic excised, only ECCAD path remains. | High |
| `include/miem/ses_reader.hpp` + `src/ses_reader.cpp` | same paths — name retained for v1; future rename to `ECCADReader` is a follow-up PR | Lift. CF-compliant time-axis decoding: read `units` attribute, parse `"<unit> since <epoch>"`, error (`UnsupportedCalendar`) if `calendar` attribute is anything other than `gregorian`/`proleptic_gregorian`/`standard`/missing. v1 hard-rejects `noleap`, `360_day`, etc. Re-throw NetCDF errors as `IOError`. | High |
| `include/miem/temporal_interpolator.hpp` + `src/temporal_interpolator.cpp` | same paths | Lift. `SetBracket` returns `Result<void>` on size mismatch. `Interpolate` is total (no throws) — accepts any time and clamps in linear mode. Caller (`OfflineEmissionSource`) handles out-of-range gating per D5. | High |
| `include/miem/flux_converter.hpp` + `src/flux_converter.cpp` | same paths | Lift. Implements kg/m²/s → tendency (kg/kg/s) via host-supplied `air_density` and `layer_thickness`. Document the unit contract in the header. `Apply` returns `Result<void>` and validates input array sizes. Add a runtime mass-conservation check gated by `MIEM_CHECK_MASS_CONSERVATION` (default ON in Debug, OFF in Release). | High |
| `include/miem/species_map.hpp` + `src/species_map.cpp` | same paths | Lift the **programmatic-only** surface. Delete `SpeciesMap(yaml_path)` ctor and its `#include <yaml-cpp/yaml.h>`. Add `Result<void> Validate() const` (checks scaling-factor sum ≤ 1.0 per inventory species). `Apply` returns `Result<void>`. | High |
| `include/miem/miem.hpp` | `include/miem/miem.hpp` | Umbrella include of public C++ headers. | Low |
| `include/miem/miem_c.h` | `include/miem/miem_c.h` | **Full rewrite per arch doc §5.** See "C API design" section. | High |
| `src/c_interface/miem_c_interface.cpp` | same path | Implement the new setter-based config builder and struct-taking `CreateMIEM`. Boundary helper `unwrap_or_set(Result<T>, MIEM_Error*)` for Result-to-error-struct translation. Drop path-taking entry. | High |
| `src/c_interface/emis_state_c_interface.cpp` | same path | Keep accessor pattern from scaffolding. Hook into new `EmisState` flat buffers. | High |
| `src/c_interface/error_handling.hpp` | same path | Replace with `unwrap_or_set` template + `MIEM_Error` population helpers. | High |
| `src/config.cpp` (scaffolding's `FromYAML`) | **repurpose** | Schema parsing has moved to MechanismConfiguration, but the file is retained in the port as the home for `MIEMConfig::Validate()`. No YAML / no-MC includes remain. | High |
| `fortran/*` | **delete entire `fortran/` subtree** | Constraint: NO Fortran; musica handles bindings. | High |
| `configs/example_*.yaml` | **delete** | Schema examples now live in MC under `examples/emissions/v1/`. | High |
| `cmake/MIEMConfig.cmake.in` | `cmake/MIEMConfig.cmake.in` | Update for new export targets (`musica::miem`, `musica::miem_c`). Include `CMakeFindDependencyMacro` for `netCDF`. No yaml-cpp dep. | High |
| (new) `src/scope-uncertain/README.md` | same path | Document: "These files exist to land scaffolding code that may belong in v1 but is not in scope for this port. Each subdirectory README explains the gate (config field, runtime check) that determines whether the code is dead in v1." | High |
| `test/` (entire) | **don't touch** | Test surface is specified below; a separate test-writer agent produces the tests. | Med |

## Architecture & engineering decisions

### Type system
- `Real` typedef preserved. `using Real = double` when `MIEM_USE_DOUBLE` defined (default ON), `float` otherwise. Used throughout core kernels.
- C API is always double. `static_assert(std::is_same_v<Real, double>)` lives inside `src/c_interface/miem_c_interface.cpp`, not the public C header. `MIEM_USE_DOUBLE=OFF` excludes `miem_c` from build.
- No `Real*` in C API. All buffer-returning C API entries use `double*`.

### Error handling
- Public boundary: `Result<T>`. Internal: exceptions allowed, caught at public entries. `Result<void>` specialization. Modeled exactly on `mechanism_configuration::ParserResult`.
- C API translation via boundary helper: `template <typename T> bool unwrap_or_set(Result<T>& r, MIEM_Error* err, T& out)` — returns true on success.

### Library structure
Three CMake targets:
- `miem_objects` OBJECT — all core `.cpp` except C interface
- `miem` STATIC — links `miem_objects` + `netCDF::netcdf_normalized PRIVATE`, alias `musica::miem`
- `miem_c` STATIC — links `miem_objects` + C-interface sources, alias `musica::miem_c`

Both `miem` and `miem_c` install. They share the OBJECT lib; do not link each other.

### Header hygiene
- Public headers: no `<yaml-cpp/...>`, no `<mechanism_configuration/...>`, no `<netcdf>` or `<netcdf.h>`. Forward-declare where possible.
- Internal headers under `src/internal/` if needed (not on install path).
- C API header: C90-compatible, only `<stddef.h>` from C stdlib.

### Public struct field naming
Trailing underscores retained (D2). Document deliberate divergence inline.

## Build system

### Root `CMakeLists.txt`
- Drop `add_library(miem INTERFACE)`. Move target declarations into `src/CMakeLists.txt`.
- Add options: `MIEM_USE_DOUBLE` (ON), `MIEM_INSTALL` (ON), `MIEM_CHECK_MASS_CONSERVATION` (default `$<CONFIG:Debug>`).
- Keep PR #7 options: `MIEM_ENABLE_TESTS`, `MIEM_ENABLE_MEMCHECK`, `MIEM_ENABLE_COVERAGE`, `MIEM_BUILD_DOCS`.
- Install rules guarded by `if(MIEM_INSTALL AND PROJECT_IS_TOP_LEVEL)`.

### `cmake/dependencies.cmake`
- Remove `find_package(yaml-cpp)` and FetchContent fallback.
- Keep `find_package(netCDF)`; FetchContent fallback to musica's pinned tag.
- Create `netCDF::netcdf_normalized` IMPORTED INTERFACE wrapping Conda/Homebrew/system/FetchContent variants.

### `src/CMakeLists.txt`
```cmake
MIEM_CORE_SOURCES = emissions_module.cpp source_offline.cpp source_factory.cpp
                    species_map.cpp temporal_interpolator.cpp flux_converter.cpp
                    ses_reader.cpp util/error.cpp emis_state.cpp
MIEM_C_API_SOURCES = c_interface/miem_c_interface.cpp
                     c_interface/emis_state_c_interface.cpp

add_library(miem_objects OBJECT ${MIEM_CORE_SOURCES})
target_include_directories(miem_objects PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(miem_objects PUBLIC cxx_std_20)
target_link_libraries(miem_objects PRIVATE netCDF::netcdf_normalized)
if(MIEM_USE_DOUBLE)
  target_compile_definitions(miem_objects PUBLIC MIEM_USE_DOUBLE)
endif()

add_library(miem STATIC)
target_link_libraries(miem PUBLIC miem_objects netCDF::netcdf_normalized)
add_library(musica::miem ALIAS miem)

if(MIEM_USE_DOUBLE)
  add_library(miem_c STATIC ${MIEM_C_API_SOURCES})
  target_link_libraries(miem_c PUBLIC miem_objects PRIVATE netCDF::netcdf_normalized)
  add_library(musica::miem_c ALIAS miem_c)
endif()
```

### Install / config-package
Full `find_package(miem)` support per arch doc patterns. `MIEMConfig.cmake.in` does `find_dependency(netCDF)`, no yaml-cpp.

## C API design

`include/miem/miem_c.h` rewritten per arch doc §5. Setter-based config builder, struct-taking `CreateMIEM`, no path-taking entries.

```c
typedef struct miem_config_t miem_config_t;
typedef struct miem_source_spec_t miem_source_spec_t;  /* visible POD */
typedef struct miem_t miem_t;
typedef struct miem_state_t miem_state_t;

typedef struct {
  int code;
  char category[64];
  char message[256];
} MIEM_Error;

/* Config building */
miem_config_t* miem_config_new(void);
void           miem_config_delete(miem_config_t*);
void           miem_config_set_version(miem_config_t*, const char* version);
void           miem_config_set_regridding_none(miem_config_t*);
int            miem_config_add_source(miem_config_t*, const miem_source_spec_t*, MIEM_Error*);
int            miem_config_add_species_mapping(miem_config_t*, const char* source_name,
                                               const char* inventory_species,
                                               const char* mechanism_species,
                                               double scaling_factor, MIEM_Error*);
int            miem_config_validate(const miem_config_t*, MIEM_Error*);

/* Lifecycle */
int  CreateMIEM(const miem_config_t* cfg, int n_cells, int n_vert_levels,
                miem_t** handle, MIEM_Error*);
void DeleteMIEM(miem_t*);

/* Run */
int MIEMRun(miem_t*, double time_sec, double dt_sec,
            const double* air_density, const double* layer_thickness, int n_atm_elements,
            miem_state_t** state_out, MIEM_Error*);
void DeleteMIEMState(miem_state_t*);

/* Introspection + accessors omitted for brevity — see synthesizer output */
```

## Implementation sequence

| Step | Description | Effort |
|---|---|---|
| 1 | Delete: `fortran/`, `configs/`, `FromYAML` declarations. (Note: `src/config.cpp` was retained, not deleted -- it now hosts `MIEMConfig::Validate()`.) | 1h |
| 2 | Quarantine: move `dataset_descriptor.{hpp,cpp}` to `src/scope-uncertain/` + README | 1h |
| 3 | Build system rewrite: dependencies, 3-target split, install rules, MIEMConfig.cmake.in | 4h |
| 4 | Types & errors: finalize `util/types.hpp`, expand `util/error.hpp` with `Result<T>` + `ErrorCode` | 3h |
| 5 | Config types: finalize `config.hpp`, add `Validate()` | 4h |
| 6 | Lift kernels: `species_map`, `temporal_interpolator`, `flux_converter`, `emis_state` | 6h |
| 7 | Lift I/O: `ses_reader` with CF-calendar enforcement | 4h |
| 8 | Lift sources: `source`, `source_offline` (kill wrap), `source_factory` | 4h |
| 9 | `EmissionsModule`: real header + lifted `.cpp`, HEMCO resolver | 6h |
| 10 | C API: rewrite header + impl + accessors | 6h |
| 11 | Header hygiene sweep: IWYU, banned-include grep | 2h |
| 12 | Install verification: cmake --install + scratch downstream consumer | 2h |
| 13 | CLAUDE.md/README touch-up | 0.5h |

**Total: ~43h ≈ 5–7 engineer-days.**

## Test surface (for the test-writer agent)

### Smoke / build
- S1: Builds with default options
- S2: Builds with `MIEM_USE_DOUBLE=OFF` (no `miem_c`)
- S3: `find_package(miem)` works after install
- S4: `grep -rE '<(yaml-cpp|mechanism_configuration)/' include/ src/` returns empty

### `Result<T>` + error boundary
- R1: `Result::Ok(v)` truthy with correct value; `Result::Error(c,m)` falsy with correct code
- R2: `MIEMError` thrown inside kernel + caught at boundary → `Result.errors[0]` populated correctly
- R3: `unwrap_or_set` populates `MIEM_Error` correctly on success/failure

### `MIEMConfig::Validate`
- V1: Rejects `regridding_.type_ != kNone` with `UnsupportedRegriddingType`
- V2: Rejects non-`"eccad"` convention with `UnknownConvention`
- V3: Rejects `mode_ == Online` with `OnlineSourcesNotSupported`
- V4: Rejects `vertical_injection_ != Surface` with `UnsupportedVerticalInjection`
- V5: Rejects scaling-factor sum > 1.0 + 1e-6 with `SpeciesMapScalingExceedsOne`
- V6: Succeeds for minimal valid config

### `SpeciesMap::Apply` mass conservation
- A1: `{NOx → NO 0.9, NOx → NO2 0.1}` applied to 1 kg/m²/s NOx, 4 cells → NO=0.9, NO2=0.1 per cell, sum=1.0 exactly
- A2: Identity mapping output equals input
- A3: Under-unity sum (0.5): output sums to 0.5 input; missing 50% silently dropped
- A4: Inventory species not in map: silently dropped
- A5: Size-mismatch input → `Result::Error{CellCountMismatch}`

### `SESReader` / time decoding
- T1: `units = "seconds since 2020-01-01"` + `calendar = "gregorian"` → UNIX epoch within 1ms
- T2: `calendar = "noleap"` → `UnsupportedCalendar` error
- T3: Missing `calendar` attribute → accepted as proleptic Gregorian
- T4: Missing `units` on time var → `InvalidTimeUnits` error

### `OfflineEmissionSource`
- O1: Mid-month `time_current` between two file times → linear blend; `nearest` mode → closer end
- O2: `time_current` > `times[n-1]` → `Result::Error{TimeOutOfRange}` (climatology kill test)
- O3: `time_current` < `times[0]` → `Result::Error{TimeOutOfRange}`
- O4: Cell-count mismatch file vs param → `Result::Error{CellCountMismatch}`
- O5: `{YYYY}-{MM}.nc` + `2024-03-15T00:00Z` → `2024-03.nc`

### `FluxConverter`
- F1: Uniform `air_density=1.0`, `layer_thickness=100`, `n_vert=10`, surface 1.0 → layer-1 tendency = 1e-2, others 0
- F2: Column-integral `tendency × ρ × Δz` = input surface flux within `1e-9 × surface_flux`
- F3: With `MIEM_CHECK_MASS_CONSERVATION`, perturbed conversion → `Result::Error{MassConservationViolation}`

### `EmissionsModule`
- E1: Two sources different categories → sum per-cell per-species
- E2: Two sources same category different hierarchies → higher wins; lower dropped (not summed)
- E3: Duplicate `(category, hierarchy)` → rejected by `Validate` with `DuplicateCategoryHierarchy`
- E4: `sector = "anthropogenic"` populates `sector_fluxes_` AND contributes to `surface_flux_`
- E5: End-to-end mass conservation across full pipeline (within `1e-9 × total`)
- E6: Source ordering does not change output (commutativity)

### C API
- C1: Full lifecycle `miem_config_new` → ... → `DeleteMIEM` runs Valgrind-clean
- C2: `MIEMGetSurfaceFlux` returns bit-equal values to C++ `EmisState`
- C3: Invalid config → `CreateMIEM` returns nonzero, `MIEM_Error` populated
- C4: Unknown sector name → returns NULL, populates `MIEM_Error`

### Defaults audit
Single fixture builds bare-minimum config (one source, only required fields, all others defaulted); assert each default's value.

## Acceptance criteria

The port is done when **all** are true:

1. `cmake -B build && cmake --build build` succeeds clean tree, default options, macOS-arm64 and Linux-x86_64
2. `grep -rE '<(yaml-cpp|mechanism_configuration)/' include/ src/ CMakeLists.txt cmake/` returns empty
3. `cmake --install` + downstream `find_package(miem CONFIG REQUIRED)` works for `musica::miem` and `musica::miem_c`
4. Three CMake targets exist: `miem_objects` OBJECT, `miem` STATIC, `miem_c` STATIC
5. `MIEM_USE_DOUBLE=ON` default; `Real == double` under it; `MIEM_USE_DOUBLE=OFF` builds without `miem_c`
6. No public C++ header includes `<yaml-cpp/...>`, `<mechanism_configuration/...>`, or `<netcdf[.h]>`
7. C API matches arch doc §5: setters + opaque handle + struct-taking `CreateMIEM`. No `const char* config_path` anywhere
8. `MIEMConfig::Validate()` exists, returns `Result<void>`, enforces all failure modes V1–V5 plus `OnlineSourcesNotSupported`, `UnsupportedVerticalInjection`, `DuplicateCategoryHierarchy`
9. `SpeciesMap::Validate()` exists, called by `MIEMConfig::Validate()`, enforces ≤ 1.0 + 1e-6
10. Climatological wrap-around in `OfflineEmissionSource::LoadBrackets` removed; out-of-range → `TimeOutOfRange`
11. `SESReader` reads `units` and `calendar`, rejects unsupported calendars
12. Every public function that can fail returns `Result<T>`; no public C++ function throws
13. `fortran/` and `configs/` deleted; `src/config.cpp` retained and repurposed to host `MIEMConfig::Validate()` (no YAML or MechanismConfiguration includes remain)
14. `dataset_descriptor.{hpp,cpp}` in `src/scope-uncertain/` with README; `MIEMConfig.convention_` sentinel
15. Category/hierarchy resolver deterministic and tested as such
16. PR #8 trailing-underscore field naming preserved on all public POD structs

## Open questions for user

1. **Climatological wrap-around — confirm kill, not configure.** Plan kills it. Alternative: add `SourceConfig::climatology_mode = {kNone, kAnnualWrap}` field now. Recommendation: kill now, add field when a real user asks.

2. **`MIEM_CHECK_MASS_CONSERVATION` default.** Plan: ON in Debug, OFF in Release. Alternative: always-ON for safety. Recommendation: keep as proposed.

3. **`SESReader` → `ECCADReader` rename.** Plan: retain `SESReader` name in this port, rename in follow-up PR. Alternative: do rename now. Recommendation: defer to keep diff focused.

4. **`Real = float` C API path.** Plan: disable `miem_c` when `MIEM_USE_DOUBLE=OFF`. Alternative: build with float→double boundary copy. Recommendation: defer.

5. **CF-calendar policy.** Plan accepts `gregorian`/`proleptic_gregorian`/`standard`/missing; rejects `noleap`/`360_day` with `UnsupportedCalendar`. Treating as yes-by-default.

## Known risks

**Scientific:** float drift under `MIEM_USE_DOUBLE=OFF`; HEMCO resolver cost at high source/cell counts; `FluxConverter` conservation tolerance under non-uniform geometry.

**Structural:** INTERFACE→STATIC is a one-way door; `Validate()` is the only schema validation in MIEM (programmatic callers must call it); NetCDF target normalization is highest-risk CMake item.

**Ecosystem:** Trailing-underscore divergence from MICM may attract future style audit; C API setter explosion is verbose by design; MIEM's port is independent of MC's emissions-parser release schedule (architectural payoff).
