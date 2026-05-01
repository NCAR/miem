# MIEM API Example

A worked example that constructs a `miem::MIEMConfig` programmatically
and runs one emissions timestep. This mirrors how a non-musica caller
— a direct host integration, a test harness, or a standalone tool —
wires up the Model Independent Emissions Module.

The example is the pure-C++ analog of MICM's `foo_chem.cpp` README
example. It uses MIEM-owned types exclusively: no `yaml-cpp`, no
`<mechanism_configuration/…>`, no `<musica/…>`. See
[`../docs/config-architecture.md`](../docs/config-architecture.md) for
the reasoning behind that split — MIEM deliberately does not know
about YAML; parsing lives in `NCAR/MechanismConfiguration::emissions::v1::`
and reference resolution lives in `NCAR/musica::emissions::Translate()`.

> **Note.** The headers in `include/miem/` are stubs — they define the
> API surface and compile cleanly, but contain no real file I/O or flux
> computation. Output values are placeholders. Real implementations will
> replace these stubs as MIEM is built out.

## Using the MIEM API

The following example exercises a fictitious emissions scenario on an
MPAS-A 120 km global mesh driving MOZART-T1 chemistry:

- **`cams anthro`** — global CAMS v6.2 anthropogenic, baseline everywhere.
- **`nei us anthro`** — NEI 2020 US overlay, wins over CAMS in US cells via higher hierarchy.
- **`ceds us override`** — CEDS 2021 regional override, wins over both.
- **`finn fires`** — FINN 2.6 biomass burning, daily files.
- **`megan offline climo`** — monthly biogenic climatology.

Species mapping and inventory translation are handled upstream by
MechanismConfiguration and `musica::Translate()` before this config
reaches MIEM.

Plus aspirational v2/v3 source shapes — plume-rise fires, online
dust/sea-salt/lightning/MEGAN — shown under `#if 0` so the API surface
is visible today without triggering the v1 hard-error policy.

The complete program lives in
[`full_example.cpp`](./full_example.cpp) (~400 lines). The following
excerpts show the three most essential pieces. For the full code
including species maps, the CEDS descriptor, all five live sources,
and the aspirational `#if 0` blocks, read the file directly.

**Setting up one source** (from [`full_example.cpp`](./full_example.cpp)):

```c++
SourceConfig cams_anthro{
  .name_                   = "cams anthro",
  .mode_                   = SourceMode::Offline,
  .type_                   = SourceType::Anthropogenic,
  .file_pattern_           = "/glade/campaign/acom/emissions/cams-v6.2/"
                             "CAMS-GLOB-ANT_v6.2_{YYYY}-{MM}.nc",
  .temporal_interpolation_ = TemporalInterpolation::Linear,
  .vertical_injection_     = VerticalInjection::Surface,
  .category_               = 0,
  .hierarchy_              = 1,
  .scaling_factor_         = 1.0,
  .sector_                 = "anthropogenic",
};
```

**Assembling the config and constructing the module**:

```c++
MIEMConfig cfg{
  .version_ = "1.0.0",
  .sources_ = { cams_anthro, nei_us_anthro, finn_fires,
                ceds_us_override, megan_offline_climo },
};

const int n_cells       = 163842;
const int n_vert_levels = 60;
EmissionsModule module(cfg, n_cells, n_vert_levels);
```

**Running a timestep and reading the output**:

```c++
const double sim_time_sec = 86400.0 * 180.0;  // day 180 of a 2025 run
const double dt_sec       = 600.0;             // 10 minutes
EmisState state = module.Run(sim_time_sec, dt_sec);

std::cout << "cell 0, NO  surface flux: "
          << state.surface_flux_(0, "NO") << " kg m-2 s-1\n";

// Diagnostic sector bucket — cams_anthro, nei_us_anthro, and
// ceds_us_override all contribute to "anthropogenic" because they
// share that sector label.
std::cout << "sector 'anthropogenic', cell 0, NO: "
          << state.sector_fluxes_.at("anthropogenic")(0, "NO")
          << " kg m-2 s-1\n";
```

**To build and run the example** (from this directory):

```
g++ -o full_example full_example.cpp -I../include -std=c++20
./full_example
```

**Expected output** (numbers are placeholders — MIEM's runtime is not
implemented on this branch):

```
     cell,     species,                 flux [kg m-2 s-1]
  -------, -----------, -------------------------------
        0,          NO,                      2.147e-10
        0,         NO2,                      2.385e-11
        0,          CO,                      5.032e-09
        0,        ISOP,                      1.806e-10

sector 'anthropogenic', cell 0, NO (CAMS+NEI+CEDS bucket): 2.147e-10 kg m-2 s-1
sector 'fire',          cell 0, CO (FINN):                 1.204e-10 kg m-2 s-1
sector 'biogenic',      cell 0, ISOP (MEGAN climo):        1.806e-10 kg m-2 s-1
```

## What the Example Covers

Each numbered section in `full_example.cpp` maps to a MIEM capability
and to an architecture-doc section:

| `.cpp` section                          | MIEM capability demonstrated                                                             | Architecture-doc reference |
| --------------------------------------- | ---------------------------------------------------------------------------------------- | -------------------------- |
| 1. Sources (live)                       | `Offline` mode; `Anthropogenic` / `Fire` / `Biogenic` types; file-pattern tokens         | §3, §6                     |
|                                         | Category/hierarchy layering (three anthro sources at `c=0, h=1/2/3`)                     | §7 (load-time invariants)  |
|                                         | Shared sector label across sources (diagnostic sum)                                      | §7 (open question below)   |
|                                         | `TemporalInterpolation::Linear` and `::Nearest` both exercised                           | §6                         |
| 2. Aspirational v2/v3 (`#if 0`)         | `VerticalInjection::Plume` (plume rise); `SourceMode::Online` + `provider_` (dust, sea-salt, lightning NOx, MEGAN) | §6 ("aspirational"), §9    |
| 3. `MIEMConfig` assembly                | Plain struct; no yaml-cpp, no MC types; source order does not imply precedence           | §3                         |
| 4. `EmissionsModule` construction       | Struct-taking ctor `(cfg, n_cells, n_vert_levels)`                                       | §5                         |
| 5. `Run(sim_time, dt)`                  | Timestep entry point (signature is an open question — see below)                          | §5                         |
| 6. `EmisState` consumption              | `surface_flux_`, `tendency_` (zero-filled here), `sector_fluxes_` by label                | §4 (pipeline), open Qs     |

Every v1-loadable field from architecture doc §7 is exercised in the
live (non-`#if 0`) path. Every v1-hard-error construct from §9
(`OnlineSourcesNotSupported`, `UnsupportedVerticalInjection`) has a
matching `#if 0` block tied to a concrete line in the example.

## Aspirational Constructs

Every aspirational construct in the example is wrapped in `#if 0` and
raises a hard error under v1 semantics. Policy (architecture doc §6):
no silent accept-and-ignore.

| Construct in the example                               | v1 runtime error                     | Runtime component that unblocks it |
| ------------------------------------------------------ | ------------------------------------ | ---------------------------------- |
| `.vertical_injection_ = VerticalInjection::Plume`      | `UnsupportedVerticalInjection`       | `PlumeRise` (v2)                   |
| `.mode_ = SourceMode::Online` (dust)                   | `OnlineSourcesNotSupported`          | `OnlineDustSource` (v2, via QUACS) |
| `.mode_ = SourceMode::Online` (sea-salt)               | `OnlineSourcesNotSupported`          | `OnlineSeaSaltSource` (v2, via QUACS) |
| `.mode_ = SourceMode::Online` (lightning NOx)          | `OnlineSourcesNotSupported`          | `OnlineLightningSource` (v3, via QUACS) |
| `.mode_ = SourceMode::Online` (MEGAN)                  | `OnlineSourcesNotSupported`          | `OnlineBiogenicSource` (v3, via QUACS) |

## Open Design Questions

The act of writing this example surfaced decisions the architecture doc
does not yet pin. Each item below names one decision the user needs to
adjudicate before MIEM's public headers are authored. (Questions about
*style* — C++20, trailing-underscore fields, aggregate init — are
deliberately omitted; those are pinned by matching MICM.)

1. **`Run(sim_time, dt)` vs `Run(dt)` vs `Step(state)`.** The example
   uses `module.Run(sim_time_sec, dt_sec)`. Alternatives: `Run(dt)`
   with MIEM holding its own clock, or `Step(state)` that takes a
   full state struct and returns an updated one. Each has different
   caller ergonomics (MPAS-A, CAM, direct callers). The architecture
   doc does not pin this.

2. **`sim_time` reference: absolute or model-relative?** If
   `Run(sim_time, dt)` wins, the example assumes `sim_time_sec` is
   absolute seconds since a model epoch (so MIEM can read the CF-1.8
   `time:units = "<unit> since <epoch>"` attribute on the input
   NetCDF and align). An alternative is model-relative: MIEM knows
   its epoch and the host just passes "elapsed seconds since start".

3. **`EmisState` access pattern.** The example uses
   `state.surface_flux_(cell, species_name)` — a functor with a
   string key. Alternatives: raw multi-dim arrays indexed by
   resolved species index (`state.surface_flux_[cell][species_idx]`),
   a `std::span` + separate index map, or Kokkos views if MIEM targets
   GPU dispatch. String keying at read time is ergonomic but slower;
   for hot-loop host code a resolved-index API is likely required.

4. **Sector diagnostics: always-on or opt-in?** The example reads
   `state.sector_fluxes_.at("anthropogenic")(0, "NO")` unconditionally.
   Populating sector buckets costs memory (`n_sectors × n_cells ×
   n_species`). Options: (a) always compute, pay the cost; (b) opt-in
   via a `MIEMConfig` flag; (c) opt-in per sector by listing which
   labels to materialize. The example uses (a) provisionally.

5. **`sector` labels: sum or error on duplicates across sources?**
   `cams_anthro`, `nei_us_anthro`, and `ceds_us_override` all share
   `.sector_ = "anthropogenic"` in this example. If duplicate labels
   sum at the sector bucket, the diagnostic shows the total anthro
   flux — useful. If duplicate labels are a load-time error, the
   diagnostic wants unique per-source labels. The example assumes
   summing; confirm.

6. **Is the set of `sector` strings free-form or pinned?** Free-form
   lets users invent labels for their diagnostics; pinning to a known
   list makes downstream tooling reason about them. The example uses
   `"anthropogenic"`, `"fire"`, `"biogenic"`, and aspirational
   `"dust"`, `"seasalt"`, `"lightning"` — picks itself a de-facto
   list. Confirm or loosen.

7. **Inlined vs registry `SpeciesMap` / `DatasetDescriptor`.** The
   example inlines a `SpeciesMap` into each `SourceConfig`. The
   architecture doc §3 shows `MIEMConfig` holding sources only (no
   named registry on MIEM's side — named species-maps and descriptors
   live on the MC-emissions side and are resolved by musica's
   `Translate()` before reaching MIEM). Confirm that on MIEM's side
   the inlined form is canonical — i.e., `MIEMConfig` does **not**
   carry parallel `species_maps_`/`descriptors_` maps that sources
   reference by name.

8. **`SourceType` enum values.** Used in this example: `Anthropogenic`,
   `Fire`, `Biogenic`, and (aspirational) `Dust`, `SeaSalt`,
   `Lightning`. Architecture doc §6 shows free-form YAML strings
   (`type: anthropogenic`, `type: fire`). The C++ translation could
   be (a) a scoped enum with a fixed set, (b) a free-form string on
   MIEM's side too (matching YAML). Enum is safer for runtime dispatch.
   Confirm the value list — does v1 need `Volcanic`, `Aircraft`,
   `Agricultural`?

9. **`provider_` string shape for online sources.** The aspirational
   `#if 0` block uses `"quacs.dust"`, `"quacs.seasalt"`, etc.
   (namespace.type). Alternatives: flat enum (`QUACS_DUST`), nested
   struct (`{.name_="quacs", .scheme_="dust"}`). Decision can defer
   to the v2 PR, but the example had to pick a shape.

10. **File-pattern token set.** The example uses `{YYYY}`, `{MM}`,
    `{DD}`, `{DDD}` (day-of-year), and `{sector}`. Architecture doc
    §6 shows `{YYYY}-{MM}` and `{YYYY}{MM}{DD}` only. Pin the canonical
    token list (and whether `{HH}`, `{DoW}`, etc. are reserved) as
    part of the MIEM reader spec.

11. **`EmissionsModule(cfg, …)` construct-time behavior.** The example
    assumes construction is lazy — no file I/O, no NetCDF opens, no
    regridding-weights read. Files open on the first `Run()` that
    needs them. Architecture doc §7 says "filesystem and numerical
    checks at runtime"; that could mean either construct-time or
    first-run. Example chooses first-run.

12. **ECCAD time-coordinate assumptions.** MIEM's `DatasetDescriptor`
    has a `time_dimension_` field for non-ECCAD files. ECCAD-conforming
    files have no equivalent knob in `SourceConfig`, implying a
    hardcoded assumption (likely CF-1.8 `"time"` with `units = "<unit>
    since <epoch>"`). Either document the hardcode or add an optional
    `time_dimension_` to `SourceConfig` too.

13. **`__`-prefixed metadata on structs.** MC's YAML accepts
    `__notes`, `__owner`, etc. at every level. On MIEM's C++ side
    there is no obvious home — the translator would either drop them
    or stuff them into a `std::unordered_map<std::string, std::string>
    __metadata_;` on each struct. The example does not expose any
    metadata fields. Confirm whether MIEM's structs should carry a
    metadata map (for provenance in error messages) or whether the
    translator is responsible for logging and discarding.

## Moving to the MIEM Repo Proper

When MIEM's public headers land (Migration Step 3 in the architecture
doc), this `examples/` tree becomes part of the build system — the
`g++` command in the Using section becomes a CMake `add_executable`
rule, and CI verifies the example compiles against the real headers.

The open-questions list above doubles as a checklist for the
header-authoring PR. Each decision — `Run` signature, `EmisState`
access, sector-diagnostic opt-in, enum values — closes as a concrete
field, method, or enum appears in `include/miem/`, and the example is
updated in lock-step (it is the canonical smoke test for those
decisions being ergonomic).

The example itself is MIEM-internal by design. A musica-side
YAML-to-flux example — showing `mechanism_configuration::emissions::v1::
Parser::Parse()` → `musica::emissions::Translate()` → `EmissionsModule`
— belongs in musica's repo (Migration Step 4), not here.
