# MIEM Config Architecture — Summary

**Status:** design proposal for the MC/musica YAML pipeline (not yet implemented).
The MIEM-side API it sketches has since been built differently during the
scaffolding port — see **Update (2026-06-09)** below.
**Scope:** where the MIEM config schema and its parser live, how MIEM consumes the
parsed result, and the boundary between load-time and runtime concerns.
**Non-goals:** ECCAD (the NetCDF *input file* convention in [`docs/eccad.md`](./eccad.md))
is unrelated and stays in MIEM unchanged.

---

## Update (2026-06-09) — current MIEM-side API

This document predates the scaffolding port. The **MC and musica design below —
the three-repo split, `kind:` dispatch, the yaml-cpp discipline, and the
load-time vs runtime boundary — still stands.** But the MIEM-side specifics it
sketches have changed; read the MIEM-side type and function names through these
substitutions:

- **No aggregate config type.** There is no `miem::MIEMConfig` / `miem::EmissionsConfig`.
  Mirroring micm (which has no `Config` type), MIEM exposes the per-source
  description `miem::Source` (the `micm::Process` analog) and a fluent
  **`miem::EmissionsBuilder`**:

  ```cpp
  miem::Emissions emissions = miem::EmissionsBuilder()
                                  .SetGridDimensions(n_cells, n_vert_levels)
                                  .AddSource(src)        // src is a miem::Source
                                  .Build();
  ```

  `Build()` returns an `Emissions` by value and **performs all validation**
  (regridding / convention / mode / vertical injection, species-map scaling,
  duplicate `(category, hierarchy)`), throwing on the first failure — exactly as
  micm folds validation into its `CpuSolverBuilder::Build()`. There is no public
  `Validate()` and no `Emissions::Create()`.
- **Description types live in `include/miem/source_types.hpp`** (formerly
  `config.hpp`; there is no `config.cpp`): the `Source` struct, the source enums,
  and `Regridding` (now a builder setting, not an aggregate field).
- **Runtime class is `miem::Emissions`** (not `EmissionsModule`), and it is
  move-only, constructed only by `EmissionsBuilder`. The NetCDF reader is
  **`ECCADReader`** (not `SESReader`), kept in `src/internal/`.
- **No C API in MIEM.** MIEM is C++-only; the `extern "C"` / Fortran wrapper
  layer lives in `NCAR/musica` (per the PR #9 review). So musica's translator
  produces a `std::vector<miem::Source>` and drives `EmissionsBuilder` directly —
  there is no `CreateMIEM(const char*)` or struct-taking C entry point in MIEM.
- **Errors are exceptions.** MIEM throws `miem::MiemException(category, code,
  msg)` (modeled on `micm::MicmException`); the `Result<T>` / `MIEMError`
  scaffolding was removed. `musica::HandleErrors()` catches it at the C boundary.

The sections below are kept as the original design record.

---

## 1. The principle

> MIEM assumes its config already exists. Parsing, schema validation, and
> version dispatch live in `NCAR/MechanismConfiguration` as a new sibling
> module alongside the existing chemistry schema. Reference resolution and
> type translation live in musica. MIEM receives fully-materialized C++
> structs that it owns, and does not depend on `yaml-cpp` or on
> MechanismConfiguration.

This mirrors MICM exactly: MICM is a header-only solver with no `yaml-cpp`
dependency and no `MechanismConfiguration` dependency. `NCAR/musica` is the
integration layer — it pulls both in, parses YAML via MechanismConfiguration,
translates the parsed result into the `micm::System` type MICM consumes, and
hands it over. Applying the same factoring to MIEM keeps hosts that embed
MIEM free of transitive YAML pulls, decouples the schema release cadence
from MIEM's, and keeps MIEM's public surface free of upstream schema types.

MIEM's schema lives *inside* MechanismConfiguration (not in a separate
`EmissionsConfiguration` repo) under a new sibling namespace:
`mechanism_configuration::emissions::v1`. Chemistry stays at
`mechanism_configuration::v1`. The two share MC's top-level primitives
(`ParserResult`, `ConfigParseStatus`, `Version`, `errors`,
`validate_schema`) directly — no vendoring, no coupling across repos for
schema work, since everything schema-related is now one repo.

## 2. Which repo owns what

MechanismConfiguration today is **chemistry-scoped**: its namespace is
`mechanism_configuration`, every type name is `Species` / `Phase` /
`Reactions`, and the repo description reads *"A description of the CAMP
chemistry mechanism configuration, with examples."* Adding MIEM's schema
as a sibling module inside it requires:

1. A new sibling namespace `mechanism_configuration::emissions` (parallel to the unnamed-but-chemistry-assumed types at the top level and the `v1::`, `development::` trees).
2. Extending `UniversalParser` to dispatch on a new top-level `kind:` field, then the existing `version:` field — currently it dispatches on `version:` alone and assumes chemistry.
3. A new top-level `examples/emissions/` directory and `test/unit/emissions/` mirror.
4. Flagging to the MC maintainers that the repo name has outgrown "Mechanism" — a long-term rename (`MusicaConfiguration`?) is worth discussing, though not blocking.

**Target layout — extensions to MechanismConfiguration:**

```
MechanismConfiguration/                                       # existing repo
├── include/mechanism_configuration/
│   ├── errors.hpp, parse_status.hpp, parser.hpp,             # existing, extended
│   ├── parser_result.hpp, version.hpp, validate_schema.hpp,  # SHARED — used
│   │                                                         #   by both schemas
│   ├── v0/, v1/, development/                                # chemistry; unchanged
│   └── emissions/                                            # NEW
│       ├── v1/
│       │   ├── parser.hpp       # emissions::v1::Parser
│       │   ├── mechanism.hpp    # emissions::v1::types::EmissionsConfig
│       │   ├── types.hpp        # SourceDescriptor, SpeciesMap,
│       │   │                    # DatasetDescriptor, Regridding, Inventory
│       │   └── validation.hpp
│       └── development/         # future, for in-flight schema bumps
├── src/
│   ├── parser.cpp                                            # UniversalParser,
│   │                                                         #   extended to read
│   │                                                         #   `kind:` first
│   ├── v0/, v1/, development/                                # chemistry; unchanged
│   └── emissions/v1/parser.cpp, mechanism_parsers.cpp        # NEW, yaml-cpp PRIVATE
├── examples/
│   ├── v1/full_configuration.{yaml,json}                     # existing chemistry
│   └── emissions/v1/full_configuration.{yaml,json}           # NEW
└── test/unit/
    ├── v0/, v1/, development/                                # chemistry; unchanged
    └── emissions/v1/                                         # NEW
        ├── test_parse_sources.cpp
        ├── test_parse_species_map.cpp
        └── v1_unit_configs/…       # fixtures
```

**Primitives: shared, not vendored.** Because chemistry and emissions now
live in one repo, `mechanism_configuration::ParseStatus`, `ParserResult<T>`,
`Version`, `Errors`, and `validate_schema` helpers are consumed by both
schemas directly from the top-level `include/mechanism_configuration/`
headers. Adding new `ConfigParseStatus` enum values for emissions (e.g.,
`DuplicateCategoryHierarchy`, `UnknownConvention`) goes in the same file as
the chemistry statuses — they share one enum.

**File-kind disambiguator.** Every config file declares `kind:` at the top
level. The `UniversalParser` rejects files without a recognized `kind:` as
`NotAConfigKind` before touching `version:`. Values: `kind: emissions`
(routes to `emissions::v1::Parser`); `kind: mechanism` (chemistry — new
field, MC adds it for symmetry; files missing `kind:` are assumed
chemistry for backward compatibility, same fallback rule MC already
applies to missing `version:`). A chemistry YAML accidentally handed to
MIEM's integration path fails fast with a clear `NotAnEmissionsConfig`
before version dispatch runs.

**Three-repo responsibility split:**

| Repo | Owns |
| --- | --- |
| `NCAR/MechanismConfiguration` (existing, extended) | Chemistry schema (unchanged) AND new emissions schema under `mechanism_configuration::emissions::v1`. `UniversalParser` extended to dispatch on `kind:`. Shared primitives (`ParserResult`, `ConfigParseStatus`, `Version`, `validate_schema`). All load-time invariants for both schemas. Yaml-cpp lives *only* here. |
| `NCAR/musica` (existing) | The translator **and** the C/Fortran wrapper layer. Consumes `mechanism_configuration::emissions::v1::types::EmissionsConfig`, resolves named references (`inventory: "cams global anthro"` → concrete `{directory, file_pattern, convention}`), produces a `std::vector<miem::Source>`, and drives `miem::EmissionsBuilder` to build the runtime module. |
| `NCAR/MIEM` | Pure-data description types (`Source`, `SpeciesMap`, `Regridding`); the `EmissionsBuilder` that validates and assembles them; runtime computation (`Emissions`, `SourceFactory`, `OfflineEmissionSource`, `ECCADReader`, `TemporalInterpolator`, `FluxConverter`). C++-only — no C/Fortran API (that lives in musica), no yaml-cpp, no dependency on MechanismConfiguration. |

**What stays in MIEM, unchanged:**

| Component | Why it stays |
| --- | --- |
| `EmissionsState`, `Emissions`, `SourceFactory`, `OfflineEmissionSource` | Runtime computation, no YAML |
| `ECCADReader` | NetCDF I/O; kept in `src/internal/` (its throwing surface stays off the public install set) |
| `docs/eccad.md` | NetCDF input-file convention, not YAML schema |
| `TemporalInterpolator`, `FluxConverter` | Runtime, no YAML |
| `Source`, `SpeciesMap`, `Regridding` description types + `EmissionsBuilder` | MIEM owns its input types and assembles them via a builder (MICM-precedent pattern); see §3 |

**What is removed from MIEM:**

| Symbol | Source of truth today | After split |
| --- | --- | --- |
| `MIEMConfig::FromYAML` (the former `src/config.cpp`) | MIEM | Removed; `src/config.cpp` no longer exists. The description types now live in `include/miem/source_types.hpp`. Equivalent parse logic: MechanismConfiguration's emissions parser parses; musica resolves. |
| `SpeciesMap(yaml_path)` ctor ([src/species_map.cpp:13](../src/species_map.cpp)) | MIEM | Removed. |
| `DatasetDescriptor::FromYAML` ([src/dataset_descriptor.cpp:13](../src/dataset_descriptor.cpp)) | MIEM | Removed. |
| `yaml-cpp::yaml-cpp` link ([src/CMakeLists.txt:37](../src/CMakeLists.txt)) | MIEM | Removed. MIEM has no yaml-cpp at any level. |
| The entire C API (`CreateMIEM`, opaque `miem_*_t` handles, Fortran ISO-C binding) | MIEM | Removed from MIEM altogether (PR #9 review): MIEM is C++-only and the C/Fortran wrapper layer lives in `NCAR/musica`. musica builds the module from C++ via `miem::EmissionsBuilder`; there is no path-taking *or* struct-taking C entry point in MIEM. |

## 3. Struct ownership — two types, one translator

**Two distinct struct families exist, on two sides of musica.**

MechanismConfiguration's emissions module defines the *rich, named-reference*
form that a user actually writes in YAML. MIEM defines the *flat, inlined*
form that its runtime consumes. Musica translates between them.

**The emissions-module root struct** (rich form, keyed collections, in
MechanismConfiguration):

```cpp
namespace mechanism_configuration::emissions::v1::types {
struct EmissionsConfig : public ::mechanism_configuration::Mechanism {
  std::string kind;                                    // always "emissions"
  std::string name;
  std::string data_root;                               // env-expanded at parse
  std::unordered_map<std::string, Inventory> inventories;
  std::unordered_map<std::string, SpeciesMap> species_maps;
  std::unordered_map<std::string, DatasetDescriptor> descriptors;
  Regridding regridding;
  std::vector<SourceDescriptor> sources;               // sources hold NAMES
};
}
```

**MIEM's flat form** (MIEM-owned, lives in `include/miem/source_types.hpp`).
There is **no aggregate struct** — mirroring micm, MIEM consumes a *collection*
of per-source `Source` descriptions, assembled through `EmissionsBuilder`.
Regridding is a builder setting, not an aggregate field:

```cpp
namespace miem {
struct Source {                  // the micm::Process analog; one per inventory
  std::string name_;
  std::string file_pattern_;                           // RESOLVED path (musica inlined it)
  std::string convention_ = "eccad";
  SpeciesMap  species_map_;                            // already exists; stays put
  SourceMode  mode_ = SourceMode::Offline;
  int         category_ = 0, hierarchy_ = 1;
  Real        scaling_factor_ = 1.0;
  std::string sector_;
  // temporal_interpolation_, vertical_injection_ …
};
}
```

**Musica's translator** (new code, in musica, not in MIEM):

```cpp
// In musica/src/emissions/translate.cpp
std::vector<miem::Source> musica::emissions::Translate(
    const mechanism_configuration::emissions::v1::types::EmissionsConfig& in)
{
  std::vector<miem::Source> out;
  for (const auto& src : in.sources) {
    const auto& inv = in.inventories.at(src.inventory);       // resolve
    const auto& smap = in.species_maps.at(src.species_map);
    miem::Source s;
    s.name_           = src.name;
    s.file_pattern_   = inv.directory + "/" + inv.file_pattern;
    s.convention_     = inv.convention;
    s.species_map_    = TranslateSpeciesMap(smap);
    s.category_       = src.category;
    s.hierarchy_      = src.hierarchy;
    s.sector_         = src.sector;
    s.scaling_factor_ = src.scaling_factor;
    // …
    out.push_back(std::move(s));
  }
  return out;
}
```

Musica then drives `miem::EmissionsBuilder` with the translated sources (and
`in.regridding`, via `SetRegridding`) to obtain the runtime `miem::Emissions`.

Musica is the only place that includes *both*
`<mechanism_configuration/emissions/…>` and `<miem/…>`. MIEM never includes
anything from MechanismConfiguration. MechanismConfiguration's emissions
module never knows MIEM exists.

**Source-location tracking.** Every parsed struct on the
MechanismConfiguration-emissions side carries an optional
`{line, column, file}` populated by the parser from yaml-cpp's `YAML::Mark`.
Cross-reference errors (e.g., `DuplicateCategoryHierarchy`,
`SourceRequiresUnknownInventory`) include locations in the formatted error
message. Without this, a user with 40 sources gets "duplicate (0, 1)" and
has to grep by hand. This is non-negotiable for usability and is a specific
place the emissions module diverges from MC's chemistry side's current
practice — the emissions parser should set the precedent, and chemistry can
adopt it later. The locations stop at the musica boundary — they are not
propagated into MIEM's flat struct, because runtime errors in MIEM are not
about source lines.

## 4. The pipeline

```
┌─────────────────────────────────────────────────────────────────────┐
│         MechanismConfiguration (emissions module, new)              │
│                                                                     │
│   miem.yaml ──▶  v1::Parser ──▶  ParserResult<EmissionsConfig>      │
│                     │             │    ├─ kind, version, name       │
│                     │             │    ├─ data_root (expanded)      │
│                     │             │    ├─ inventories{}             │
│                     │             │    ├─ species_maps{}            │
│                     │             │    ├─ descriptors{}             │
│                     │             │    ├─ regridding                │
│                     │             │    └─ sources[] ← hold NAMES    │
│                     │             └─ errors with line:col           │
│                     ▼                                               │
│           validate_schema / cross-refs / env-var expansion          │
│           (all yaml-cpp scoped to this repo)                        │
└─────────────────────────────────┬───────────────────────────────────┘
                                  │  C++ struct, yaml-cpp private
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                              musica                                 │
│                                                                     │
│   musica::emissions::Translate(EmissionsConfig)                     │
│      │                                                              │
│      │   resolves inventory/species-map/descriptor names            │
│      │   into concrete paths; flattens for MIEM's shape             │
│      ▼                                                              │
│   std::vector<miem::Source>   (+ in.regridding)                     │
│      │                                                              │
│      ▼                                                              │
│   EmissionsBuilder().AddSource(src)... .Build()  ──▶  Emissions     │
└─────────────────────────────────┬───────────────────────────────────┘
                                  │  MIEM-owned C++ types, no deps
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                              MIEM                                   │
│                                                                     │
│   Emissions  (move-only; built by EmissionsBuilder)                 │
│      │                                                              │
│      ▼                                                              │
│   ECCADReader · SourceFactory · TemporalInterpolator · Regrid ·     │
│   SpeciesMap  ──▶  EmissionsState                                   │
│                                                                     │
│   Run() ──▶ EmissionsState { surface_flux, tendency, sector_fluxes }│
└─────────────────────────────────┬───────────────────────────────────┘
                                  │  musica wraps Run for C / Fortran
                                  ▼
                            host model (MPAS, CAM, …)
```

The top block is the *entire* yaml-cpp surface in the MUSICA ecosystem.
The middle block is the only place that sees both MC-emissions and MIEM
types. MIEM has zero `#include <yaml-cpp/…>` and zero
`#include <mechanism_configuration/…>` lines.

## 5. Consumer API

MIEM exposes a builder-based C++ API only. A C wrapper for Fortran exposure lives in MUSICA, not MIEM (see PR #9 review).

```cpp
// include/miem/emissions_builder.hpp
namespace miem {
class EmissionsBuilder {
 public:
  EmissionsBuilder& SetGridDimensions(int n_cells, int n_vert_levels);
  EmissionsBuilder& SetRegridding(const Regridding&);
  EmissionsBuilder& AddSource(const Source&);
  EmissionsBuilder& SetSources(std::vector<Source>);
  Emissions Build() const;   // validates + builds; throws miem::MiemException
};
}
```

No `mechanism_configuration::…` types appear in MIEM's headers. No yaml-cpp.

### The musica-side integration (new musica code, not MIEM)

Musica is the only caller that sees both types:

```cpp
// In musica/src/emissions/create_miem_from_yaml.cpp
#include <mechanism_configuration/emissions/v1/parser.hpp>        // rich form
#include <miem/miem.hpp>                                 // flat form + builder
#include "translate.hpp"                                // the translator

musica::Result musica::emissions::CreateMIEMFromYAML(
    const std::string& config_path, int n_cells, int n_vert_levels,
    std::unique_ptr<miem::Emissions>& module_out)
{
  mechanism_configuration::emissions::v1::Parser parser;
  auto parsed = parser.Parse(config_path);
  if (!parsed) {
    for (const auto& [status, msg] : parsed.errors) {
      musica::ErrorBuffer::Push(status, msg);             // line:col preserved
    }
    return musica::Result::kConfigLoadFailed;
  }
  std::vector<miem::Source> sources = musica::emissions::Translate(*parsed);
  try {
    module_out = std::make_unique<miem::Emissions>(
        miem::EmissionsBuilder()
            .SetGridDimensions(n_cells, n_vert_levels)
            .SetSources(std::move(sources))
            .Build());                       // throws miem::MiemException
  } catch (const miem::MiemException& e) {
    musica::ErrorBuffer::Push(e.Code(), e.what());        // category/code preserved
    return musica::Result::kConfigLoadFailed;
  }
  return musica::Result::kOk;
}
```

This is the exact pattern musica already uses for MICM
(`mechanism_configuration::types::Mechanism` → `micm::System`). Key
properties reused from MechanismConfiguration (all on musica's side, never
MIEM's):

- `ParserResult<T>` with `operator bool` and an `errors` vector — no exceptions at the public API.
- `ConfigParseStatus` enum — mirror the naming (`FileNotFound`, `MissingVersionField`, `InvalidVersion`, `DuplicateSourceDetected`, `SourceRequiresUnknownInventory`, …).
- Two public entry points on the parser: `Parse(path)` and `ParseFromString(string)`. `ParseFromNode(YAML::Node)` exists in MC's chemistry parser but leaks yaml-cpp through the public header — the new emissions parser drops it from the main `parser.hpp` and, if needed at all, offers it behind a separate opt-in header not included in the default install set. The same cleanup is worth applying to the chemistry side in a later PR.
- A `UniversalParser` that first validates `kind: emissions`, then reads the top-level `version:` field and dispatches to `v1::Parser` / `development::Parser` — set up now, even with only `v1`, so future schema bumps slot in.

## 6. Aspirational README-style YAML example

This example goes in `MechanismConfiguration/examples/emissions/v1/full_configuration.yaml`
and is the worked example a MIEM user starts from. It is **aspirational**:
the v1 MVP does not implement regridding, online sources, or sector
diagnostics, but the shape is present so we do not re-litigate the schema when
those land. Any field whose machinery is not yet implemented uses a neutral
sentinel (`type: none`, empty list, omitted-when-optional).

Style follows MechanismConfiguration exactly: space-separated multi-word keys
with SI units in square brackets, `snake_case` only for identifiers that map
directly to code symbols (source names, species names). The double-underscore
prefix preserves user metadata without failing the parser.

```yaml
kind: emissions
version: 1.0.0
name: Example MIEM configuration

# --------------------------------------------------------------------------
# Environment-expandable data root. POSIX ${VAR} / ${VAR:-default} syntax.
# The parser expands here; MIEM receives a fully-resolved path string.
# A ${VAR} without a default that is unset in the environment is a load-time
# error (UndefinedEnvironmentVariable); the `:-default` below avoids that.
# --------------------------------------------------------------------------
data root: ${EMIS_DATA_ROOT:-/glade/derecho/scratch/${USER}/emissions}

# --------------------------------------------------------------------------
# Inventories: named I/O metadata. Offline sources reference by name.
# Moves per-source path/pattern/convention out of the sources list so a
# machine move is a one-line edit to `data root` above.
# --------------------------------------------------------------------------
inventories:
  cams global anthro:
    directory: cams/v6.2
    file pattern: CAMS-GLOB-ANT_v6.2_{YYYY}-{MM}.nc
    convention: eccad
  finn fires:
    directory: finn/v2.5
    file pattern: FINNv2.5_{YYYY}{MM}{DD}.nc
    convention: eccad
  ceds legacy:
    directory: ceds/v2021-04-21
    file pattern: CEDS_{sector}_{YYYY}.nc
    convention: descriptor
    descriptor: ceds legacy descriptor   # references descriptors: below

# --------------------------------------------------------------------------
# Species maps: inventory → mechanism mappings. Named, referenced by sources.
# Per-inventory scaling factors must sum to ≤ 1.0 (mass conservation).
# --------------------------------------------------------------------------
species maps:
  MOZART-T1 from CAMS:
    mechanism: MOZART-T1
    # Per-inventory-species scaling factors must sum to ≤ 1.0 (mass
    # conservation). An omitted `scaling factor` defaults to 1.0 when the
    # inventory species has exactly one mapping. Under-unity sums are allowed
    # (represents mass routed to species the mechanism does not track).
    mappings:
      - inventory species: NOx
        mechanism species: NO
        scaling factor: 0.9
      - inventory species: NOx
        mechanism species: NO2
        scaling factor: 0.1
      - inventory species: SO2
        mechanism species: SO2
        scaling factor: 0.975
      - inventory species: SO2
        mechanism species: SULF
        scaling factor: 0.025
      - inventory species: CO
        mechanism species: CO
      - inventory species: NMVOC         # sum = 0.65 — remaining 35% is
        mechanism species: BIGALK        # routed to non-MOZART-T1 species
        scaling factor: 0.3              # and silently dropped.
      - inventory species: NMVOC
        mechanism species: BIGENE
        scaling factor: 0.2
      - inventory species: NMVOC
        mechanism species: TOLUENE
        scaling factor: 0.15
      # Adding a fourth NMVOC mapping with scaling factor 0.8 here would push
      # the sum to 1.45 and fail with SpeciesMapScalingExceedsOne.

# --------------------------------------------------------------------------
# Dataset descriptors: adapt non-ECCAD NetCDF files to the canonical layout.
# Only needed for legacy inventories; ECCAD-conforming files skip this.
# --------------------------------------------------------------------------
descriptors:
  ceds legacy descriptor:
    variable prefix: emiss_
    flux units: kg m-2 s-1
    unit conversion factor: 1.0
    time dimension: time
    cell dimension: ncol
    species rename:
      emiss_no: emi_NO
      emiss_co: emi_CO
      emiss_so2: emi_SO2

# --------------------------------------------------------------------------
# Regridding: aspirational. MVP supports only `type: none` (data already on
# target grid). Future: `type: scrip` reads ESMF/SCRIP weights, applies a
# conservative SpMV + frac_b normalization.
# --------------------------------------------------------------------------
regridding:
  type: none
  # type: scrip
  # weights file: ${EMIS_DATA_ROOT}/weights/cams_to_mpas120km.nc

# --------------------------------------------------------------------------
# Sources: the emission signals themselves. Category/hierarchy is HEMCO-style.
# Sources in different categories sum; within a category the highest
# hierarchy wins per cell. Duplicate (category, hierarchy) pairs are a
# load-time error, not silent last-wins.
# --------------------------------------------------------------------------
sources:
  - name: cams anthro
    mode: offline
    type: anthropogenic
    inventory: cams global anthro
    species map: MOZART-T1 from CAMS
    temporal interpolation: linear
    vertical injection: surface
    category: 0
    hierarchy: 1
    scaling factor: 1.0
    sector: anthropogenic

  - name: finn fires
    mode: offline
    type: fire
    inventory: finn fires
    species map: MOZART-T1 from CAMS
    temporal interpolation: nearest
    vertical injection: surface       # MVP accepts only `surface`; `plume` is
                                      # a hard UnsupportedVerticalInjection
                                      # error until the plume-rise module
                                      # lands. No silent downgrade.
    category: 1
    hierarchy: 1
    sector: fire

  - name: ceds us override
    mode: offline
    type: anthropogenic
    inventory: ceds legacy
    species map: MOZART-T1 from CAMS
    temporal interpolation: linear
    vertical injection: surface
    category: 0
    hierarchy: 2            # wins over `cams anthro` where both have values
    sector: anthropogenic
    __notes: "regional override for US cells per 2026 EPA inventory"
```

**What's aspirational / not in the MVP:**

| Construct | MVP behavior |
| --- | --- |
| `regridding.type: scrip` (and `weights file`) | Parser accepts only `type: none`; `type: scrip` is `UnsupportedRegriddingType` error |
| `mode: online` on a source | `OnlineSourcesNotSupported` error |
| `vertical injection: plume` | `UnsupportedVerticalInjection` error — no silent downgrade |
| `sector` diagnostic output | Parsed, stored on `EmissionsState::sector_fluxes_`, but no diagnostic reader tool yet (consumed by any host that asks) |
| Sector-templated `file pattern` (`{sector}`) | Parsed and substituted at read time; `finn fires` example omits it; `ceds legacy` uses it |

**Policy:** every aspirational construct is a hard load-time error until its
runtime machinery lands. No silent accept-and-ignore, no warnings
downgraded to surface-equivalent. When a new version's machinery arrives,
the config-repo minor version bumps (e.g., `1.1.0`), the error becomes an
accept, and MIEM's pinned tag follows. Users who wrote forward-compatible
configs (explicit `type: none` today) keep working unchanged.

## 7. Load-time vs runtime concerns

The boundary follows MechanismConfiguration's convention: pure-schema
validation at load; filesystem and numerical checks at runtime.

| Check | Where |
| --- | --- |
| YAML syntax | Config repo (yaml-cpp internal) |
| `kind` field present and equals `emissions` | Config repo (`UniversalParser` — rejects `NotAnEmissionsConfig`) |
| Required / optional key presence, type correctness | Config repo (`validate_schema`) |
| `version` field present, known major version | Config repo (`UniversalParser`) |
| `${VAR}` / `${VAR:-default}` expansion on `data root` | Config repo (string manipulation, no I/O) |
| Unknown `${VAR}` without a default | Config repo (load-time error — silent empty expansion is forbidden) |
| Duplicate `(category, hierarchy)` across sources | Config repo (cross-ref on parsed tree; error includes both source names and their line:col) |
| Source references an inventory that is not declared | Config repo (cross-ref) |
| Source references a species map that is not declared | Config repo (cross-ref) |
| Source references a descriptor that is not declared | Config repo (cross-ref) |
| `inventory.convention` is one of the known strings (`eccad`, `descriptor`) | Config repo — string enum check; `UnknownConvention` load-time error. The actual string→reader-class dispatch lives in MIEM. |
| Species-map scaling factors sum ≤ 1.0 per inventory species | Config repo (numerical invariant, no I/O) |
| `regridding.type` is one of the known strings; v1 accepts only `none` | Config repo — `UnsupportedRegriddingType` load-time error |
| `vertical injection` is one of the known strings; v1 accepts only `surface` | Config repo — `UnsupportedVerticalInjection` load-time error |
| `mode` is one of the known strings; v1 accepts only `offline` | Config repo — `OnlineSourcesNotSupported` load-time error |
| `inventories.*.directory` resolves to an existing directory | MIEM runtime (filesystem) |
| `regridding.weights file` exists (when v1.1 lands `type: scrip`) | MIEM runtime |
| NetCDF file matches ECCAD convention, species variables present | MIEM runtime (`ECCADReader::Open`) |
| Per-cell numerical sanity (NaN, negative flux) | MIEM runtime |

## 8. Versioning

Copy MechanismConfiguration's pattern exactly. Every config file has a
top-level `version: MAJOR.MINOR.PATCH`. `UniversalParser` reads the string,
parses into `{major, minor, patch}`, dispatches:

- Missing `version:` → `MissingVersionField` error. (MC falls back to v0 — we skip that because MIEM has no v0.)
- Unknown major → `InvalidVersion` error.
- Known major with unknown minor → accept; the parser ignores unknown keys (preserving them in `__unknown_properties` is the MC pattern we inherit).

Release cadence mirrors the MICM ↔ MechanismConfiguration pattern — paired
tags. When MIEM cuts `v0.3.0` and requires new schema features, MC cuts
`v1.N.M` with the matching schema and musica's `cmake/dependencies.cmake`
pins to that tag. MIEM itself does not pin MC — only musica does.

## 9. Error handling

`ParserResult<EmissionsConfig>` with `operator bool` and an `errors` vector of
`{ConfigParseStatus, std::string}` pairs — where the message string embeds
the source location (`"<file>:<line>:<col>"`). No exceptions at the public
API. No `warnings` vector: every deviation is either accepted silently
(forward-compat unknown `__` keys) or a hard error. We deliberately reject
MC's model where some deviations could be downgraded, because silent accepts
of not-yet-implemented features are the dominant failure mode in scientific
configs.

`ConfigParseStatus` enum values to define (mirroring MC's naming):

```
FileNotFound, InvalidYAML,
NotAnEmissionsConfig /* kind: is missing or != "emissions" */,
MissingVersionField, InvalidVersion,
MissingRequiredField, UnknownField, TypeMismatch,
DuplicateSourceDetected, DuplicateCategoryHierarchy,
SourceRequiresUnknownInventory, SourceRequiresUnknownSpeciesMap,
SourceRequiresUnknownDescriptor, UnknownConvention,
SpeciesMapScalingExceedsOne, UndefinedEnvironmentVariable,
OnlineSourcesNotSupported /* v1 only */,
UnsupportedRegriddingType /* v1 accepts only `none` */,
UnsupportedVerticalInjection /* v1 accepts only `surface` */
```

MIEM-side errors (runtime I/O, numerical) are raised as
`miem::MiemException(category, code, msg)` (modeled on `micm::MicmException`);
`musica::HandleErrors()` catches it at the C boundary and maps it to a MUSICA
`Error` struct. The earlier `miem::Result` / `MIEMError` scaffolding was
removed during the port.

## 10. Testing strategy

**MechanismConfiguration (emissions subtree)** owns all parse-level tests.
Framework: GoogleTest (matches the rest of the repo). Fixture-driven: each
test loads a `test/unit/emissions/v1/v1_unit_configs/…yaml` (optionally
`.json` twin if the repo commits to dual-format support from the start) and
asserts on the parsed struct or on `errors[].first`. Example:

```cpp
TEST(EmissionsV1Parser, RejectsDuplicateCategoryHierarchy) {
  emissions::v1::Parser parser;
  auto r = parser.Parse("v1_unit_configs/duplicate_cat_hier.yaml");
  EXPECT_FALSE(r);
  EXPECT_EQ(r.errors[0].first, ConfigParseStatus::DuplicateCategoryHierarchy);
  EXPECT_NE(r.errors[0].second.find(".yaml:"), std::string::npos);  // line:col embedded
}
```

Whether to maintain JSON twins is deferred until someone asks for it. MIEM's
integration tests don't exist on the MIEM side anymore; dropping dual-format
discipline keeps fixture maintenance from doubling.

**MIEM repo** keeps only one kind of test: programmatic. Construct `Source`
descriptions directly in C++ and assemble them with `EmissionsBuilder` (no
YAML, no MechanismConfiguration types), then exercise downstream logic. This
is the pattern in `test/unit/test_source.cpp` (description defaults),
`test/unit/test_emissions_builder.cpp` (builder validation),
`test/unit/test_emissions.cpp` (HEMCO aggregation), and the end-to-end
`test/integration/test_nox_pipeline.cpp`. There are no YAML-fixture tests on
the MIEM side — parse correctness is MC's job and full YAML-to-flux
integration is musica's.

**musica repo** owns the full-flow integration tests: parse a valid YAML via
`mechanism_configuration::emissions::v1::Parser`, translate, invoke MIEM,
verify computed flux. These are the only tests that exercise "a user's real
config file produces correct output." They need a valid YAML fixture,
which they pull from MC's exported examples directory (see below) — or
keep their own integration-scenario YAML next to the test.

**Fixture plumbing.** MechanismConfiguration already installs its chemistry
examples. Extend the install rule for the new emissions subtree and export
a CMake variable:

```cmake
# MechanismConfiguration/CMakeLists.txt (new lines)
install(DIRECTORY examples/emissions/v1
        DESTINATION share/mechanism_configuration/examples/emissions/v1)
set(MechanismConfiguration_EMISSIONS_EXAMPLES_DIR
    "${CMAKE_INSTALL_PREFIX}/share/mechanism_configuration/examples/emissions"
    CACHE INTERNAL "")
```

For `FetchContent` in-tree builds, the same variable is set to
`${mechanism_configuration_SOURCE_DIR}/examples/emissions`. Musica's
`test/CMakeLists.txt` consumes `${MechanismConfiguration_EMISSIONS_EXAMPLES_DIR}`
and passes it to tests via a `-DMUSICA_EMISSIONS_FIXTURES_DIR=…` compile
definition. MIEM does not consume this variable — it has no fixtures.

## 11. Open design questions (decisions needed before implementation)

> **Superseded in part.** §11 and §12 describe the MC/musica pipeline that is
> still unbuilt, and the MIEM-side steps as they were *planned* (path-taking C
> API, `MIEMConfig`, `EmissionsModule`). MIEM has since shipped the builder API
> described in **Update (2026-06-09)** at the top, and its C API moved to
> musica. Read the MIEM-side specifics below through those substitutions; the
> MC/musica steps are unchanged.

1. **Sibling repo vs module inside MechanismConfiguration?** *Decided:* the
   schema lives *inside* MC under `mechanism_configuration::emissions::v1`.
   This deliberately accepts a mildly misleading repo name and coupled
   release cadence in exchange for: one less repo to create and maintain,
   direct (not vendored) reuse of MC's `ParserResult` / `ConfigParseStatus`
   / `Version` / `validate_schema` primitives, and a unified home for all
   MUSICA-ecosystem schemas. An eventual rename of MC (e.g.,
   `MusicaConfiguration`) is worth floating with the maintainers but is
   not blocking for this work.

2. **Integration layer: musica, not MIEM.** *Decided.* Following MICM's
   precedent exactly — MIEM takes no dependency on MechanismConfiguration.
   Musica FetchContent's both, owns the `Translate()` function, and invokes
   MIEM via its struct-based C API. Consequences:
   - MIEM's standalone testing loses a YAML-to-flux round-trip capability. MIEM integration tests construct `MIEMConfig` directly in C++; musica tests cover the full YAML → `MIEMConfig` → `EmisState` path.
   - Any non-musica caller of MIEM (MPAS direct integration, ad-hoc tooling) also constructs `MIEMConfig` programmatically. The YAML-path convenience is musica-exclusive.
   - Three repos coordinate on schema changes (see §13).

3. **yaml-cpp leak — concrete discipline for the new emissions module in MC.**
   MC currently links `yaml-cpp::yaml-cpp PUBLIC` and three of its public
   headers (`validate_schema.hpp`, `v1/mechanism_parsers.hpp`,
   `v1/utils.hpp`) do `#include <yaml-cpp/yaml.h>` or declare functions
   taking `YAML::Node` by reference. That's what made it leak. The
   emissions module must NOT repeat this — and ideally the MC PR that
   adds emissions also fixes the chemistry side. Three rules:

   - **CMake**: demote `target_link_libraries(mechanism_configuration PRIVATE yaml-cpp::yaml-cpp)` (change from PUBLIC). Transitive consumers get no yaml-cpp includes or link flags.
   - **Public headers** (under `include/mechanism_configuration/emissions/` and, ideally, the existing chemistry headers): no `#include <yaml-cpp/…>`, no `YAML::Node` in function signatures, not even as `const YAML::Node&` parameters. Where a forward declaration is needed (`ParseFromString` implementation detail), use `namespace YAML { class Node; }` inside the `.cpp` only.
   - **Implementation-only headers** (schema-validator helpers, parser internals, `YAML::Node` processors): live under `src/internal/` and are excluded from `install(DIRECTORY include/…)`. Not on the install path = no way for a consumer to accidentally pick one up.

   MICM itself doesn't consume MC today (the integration is via MUSICA),
   so MC can move to PRIVATE without breaking its real consumers. This
   makes the fix low-risk and worth bundling with the emissions-module
   addition rather than deferring.

4. **YAML key style: space-separated vs snake_case.** This doc assumes MC's
   space-separated style for consistency (`file pattern`, `scaling factor`,
   `gas phase`). This breaks the current `configs/example_*.yaml` files.
   Alternative: keep MIEM's `snake_case` and explicitly diverge from MC's
   style. Recommendation: adopt the space-separated style — stylistic
   consistency across MUSICA configs outweighs the one-time migration of
   three example files.

5. **Does MIEM need a config-dump / diagnostic writer?** MC has no writer;
   round-tripping is not supported. Recommendation: copy the user's input
   YAML verbatim into the run directory for provenance. Do not introduce a
   new JSON dependency (e.g., `nlohmann::json`) speculatively — a resolved-
   view dump can be hand-emitted as simple key-value text if and when a
   real user asks for one. Do not re-link yaml-cpp into MIEM under any
   circumstance.

## 12. Migration sequence

Staged in five steps across three repos. Each step leaves all three repos
buildable. Tests migrate alongside the code they verify.

There is no signature-stability contract for MIEM's old path-based API —
this migration breaks it deliberately. MIEM's direct callers (today: a
small set of tests, possibly in-tree tooling) move to the struct-based API
in Step 3. External hosts that loaded MIEM with a YAML path switch to
calling the musica entry point from Step 4 onward.

1. **MC hygiene PR.** Open a PR on MechanismConfiguration demoting
   `yaml-cpp::yaml-cpp` to `PRIVATE` and moving leaking headers
   (`validate_schema.hpp`, `v1/mechanism_parsers.hpp`, `v1/utils.hpp`)
   to a `src/internal/` subtree excluded from install. Extend the
   `ConfigParseStatus` enum with a placeholder for `NotAConfigKind` in
   preparation for `kind:` dispatch. Lands alone; no emissions code yet.

2. **MC emissions module.** PR against MechanismConfiguration adding:
   - `include/mechanism_configuration/emissions/v1/{parser,mechanism,types,validation}.hpp`
   - `src/emissions/v1/{parser,mechanism_parsers}.cpp` (yaml-cpp PRIVATE as per Step 1)
   - `examples/emissions/v1/full_configuration.{yaml,json}` (schema from §6)
   - `test/unit/emissions/v1/…` fixtures + GoogleTest cases for every load-time invariant in §7, including source-location tracking
   - `ConfigParseStatus` enum values for all emissions error kinds from §9
   - `UniversalParser` extension: read `kind:` first; `kind: emissions` → `emissions::v1::Parser`; `kind: mechanism` or missing-for-chemistry → existing chemistry dispatch; unknown → `NotAConfigKind`
   - CMake `install(DIRECTORY examples/emissions/v1 …)` and the `MechanismConfiguration_EMISSIONS_EXAMPLES_DIR` variable (see §10)
   - Ship a new MC tag (e.g., `v1.2.0`) that bundles this with the Step 1 hygiene change.

3. **Add the struct-based entry point to MIEM (non-breaking addition).**
   - Add `EmissionsModule(const MIEMConfig&, int n_cells, int n_vert_levels)` alongside the existing path-taking constructor.
   - Add `CreateMIEM(const miem_config_t*, …)` C API alongside the existing `CreateMIEM(const char* config_path, …)`.
   - Migrate MIEM's internal unit tests that can (e.g., `test_emis_state.cpp`) to construct `MIEMConfig` directly, confirming the new path works.
   - The YAML-path path still exists and still uses yaml-cpp. Nothing in MIEM is deleted yet. MIEM still links yaml-cpp.

4. **Wire musica to MC-emissions and MIEM's new API.**
   - Bump musica's MechanismConfiguration pin in `cmake/dependencies.cmake` to the new MC tag from Step 2.
   - Add `musica::emissions::Translate(const mechanism_configuration::emissions::v1::types::EmissionsConfig&) -> miem::MIEMConfig` under `musica/src/emissions/`.
   - Add `musica::emissions::CreateMIEMFromYAML(path, …)` that calls `emissions::v1::Parser::Parse` → `Translate` → MIEM's struct-based constructor.
   - Move MIEM's YAML → flux integration tests to musica (their natural home now).
   - Tag musica release that pins both MIEM (a known good tag) and MC at the new emissions-aware version.

5. **Delete MIEM's yaml-cpp surface.**
   - Remove `find_package(yaml-cpp)` from MIEM's root `CMakeLists.txt`.
   - Remove `yaml-cpp::yaml-cpp` from [`src/CMakeLists.txt:37`](../src/CMakeLists.txt).
   - Delete `#include <yaml-cpp/yaml.h>` from `config.cpp`, `species_map.cpp`, `dataset_descriptor.cpp`.
   - Delete the old path-taking `MIEMConfig::FromYAML`, `SpeciesMap(const std::string&)`, `DatasetDescriptor::FromYAML` declarations from the public headers.
   - Delete MIEM's YAML-fixture-driven tests. Keep the programmatic-construction tests.
   - Verify no transitive yaml-cpp link via `ldd` / `otool -L` on a `libmiem` consumer.
   - Update [`CLAUDE.md`](../CLAUDE.md) and `README.md` to point users at musica for YAML-driven setup.

## 13. Contributor workflow across three repos

Adding a field — say, `injection height [m]` on `SourceDescriptor` — is a
six-step dance:

1. **MechanismConfiguration (emissions subtree)**: add the field to `emissions::v1::types::SourceDescriptor`, extend the parser, add a fixture test. Open PR.
2. Merge; tag a new MC release (e.g., `v1.3.0`).
3. **musica**: bump the MechanismConfiguration pin to `v1.3.0`. Update `musica::emissions::Translate` to read the new field and write it through to `miem::Source`. Open PR.
4. **MIEM**: add the corresponding field to `miem::Source` (if it isn't already there), wire it into runtime use (e.g., plume-rise calculation). Open PR.
5. Merge MIEM and musica PRs. Tag both as needed.
6. Hosts that use musica to load MIEM get the new capability by bumping their musica pin.

The order matters: MC must land first, then musica and MIEM can land in
either order or together. Musica's PR cannot merge until the new MC tag
exists (FetchContent needs a real ref).

For in-flight development before the MC tag, contributors use
`emissions::development::Parser` on the MC side and point musica's
`FetchContent` at a branch rather than a tag. Musica's dependency
declaration should accept an override via
`-DMechanismConfiguration_GIT_TAG=my-branch`. When the MC PR merges and is
tagged, the override drops and the musica PR lands with a pinned version
bump in the same commit. Same pattern for MIEM fields on the musica side
(`-DMIEM_GIT_TAG=my-branch`).

### What this costs and what it buys

**Costs.** A MIEM-only contributor who just wants to add a field has to
open three PRs and coordinate tagging across three repos. This is a real
friction, especially for an early-career contributor or a collaborator from
outside the immediate MUSICA team. Documentation of the workflow in
`CONTRIBUTING.md` (in all three repos) is mandatory; in practice, most
contributors will work in one `musica-dev` checkout that has all three as
submodules or FetchContent source-tree refs.

**Buys.** Release independence. MIEM can tag `v2.0.0` (breaking runtime
change) without forcing a config schema bump. MC can tag `v1.5.0` (new
optional field, chemistry or emissions side) without forcing a MIEM
release. Musica does the integration work and picks whichever pinned
versions it wants. This is the exact pattern MICM / MC / musica run
today, extended to cover MIEM.

### Not concerns (surfaced but dismissed)

- **Performance of double-parse.** The MC-emissions → musica → MIEM
  struct translation is a once-per-run cost over structs with tens to
  hundreds of entries. Not a bottleneck.
- **MIEM's lost standalone YAML round-trip test.** MIEM no longer verifies
  "a valid YAML produces correct flux" within its own CI — musica owns that
  test now. The trade is acceptable because the test surface doesn't
  disappear, it just moves one repo up. MIEM's own tests exercise "a valid
  set of `miem::Source`s produces correct flux", which is what the physics
  cares about.

## 14. Reference: relevant upstream files

- MechanismConfiguration [README](https://github.com/NCAR/MechanismConfiguration/blob/main/README.md)
- [`v1::Parser`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/include/mechanism_configuration/v1/parser.hpp) — consumer API
- [`ParserResult`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/include/mechanism_configuration/parser_result.hpp)
- [`UniversalParser`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/include/mechanism_configuration/parser.hpp) — version dispatch
- [`parse_status.hpp`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/include/mechanism_configuration/parse_status.hpp) — error enum pattern
- [`examples/v1/full_configuration.yaml`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/examples/v1/full_configuration.yaml) — style reference
- [`src/CMakeLists.txt`](https://raw.githubusercontent.com/NCAR/MechanismConfiguration/main/src/CMakeLists.txt) — link pattern (the `PUBLIC` is the gotcha)
- [MUSICA `cmake/dependencies.cmake`](https://raw.githubusercontent.com/NCAR/musica/main/cmake/dependencies.cmake) — FetchContent pin pattern
