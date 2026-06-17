# MIEM emissions visualizer

A small, local tool to watch MIEM's emissions on a map and play through time.
It extracts the inventory's monthly slices into a compact **Zarr v3** store and
serves a static page that animates the surface flux at any time resolution —
each frame computed in the browser by the **real `miem::Emissions::Run`
pipeline, compiled to WASM** (builder + `SpeciesMap` + temporal interpolation +
HEMCO aggregation).

This is scratch tooling — it is **not** part of MIEM's build or install
surface. The C++ driver links the prebuilt `libmiem.a` and the WASM module
compiles MIEM's netCDF-free sources unchanged; nothing here changes MIEM's CMake.

![global view](screenshot.png)

## Quick start

```sh
./viz/build_and_run.sh          # build everything, then serve on :8000
# open http://localhost:8000/
./viz/build_and_run.sh serve    # just view an already-built store
```

Controls:

- **field** — a raw inventory sector (each a different geography — shipping
  lanes, power plants, residential/population) or a MIEM `SpeciesMap` output:
  `Σ all sectors → BC` (aggregation, many→one) or a `0.8·Σ` / `0.2·Σ` mass
  split. All produced live by the real `SpeciesMap::Apply`.
- **interp** — `linear` / `nearest` / `held`: MIEM's `TemporalInterpolation`
  modes (`kLinear` / `kNearest` / `kNone`), applied live per frame.
- **transport** — ⏮ start, ◀ −1 h, ▶/⏸ play, ▶ +1 h, and a speed select
  (1 day/s … ~1 month/s). The slider scrubs **hourly** across the whole span.
- **log** scale; scroll to zoom, drag to pan, double-click to reset. The legend
  **auto-rescales to each field's own range** (p10–p99.5, capped at 6 decades),
  so a small sector like power generation isn't crushed to the dark floor of the
  Σ-all-sectors scale.
- The top-right sparkline shows the area-weighted **global total** over the year
  (linear ramp vs held step through the monthly knots), with a marker that rides
  the active mode at the current time.

## How the full pipeline runs in the browser

netCDF is the *only* thing that can't easily go to WASM (it'd drag HDF5 in), and
it's the one step with no scientific content — just byte-parsing. So it happens
**once, offline** in `miem_run` to extract the 12 masked monthly slices
(NaN/`_FillValue` → 0, via the real reader). Everything downstream —
`EmissionsBuilder`, `Emissions::Run`, `OfflineEmissionSource`
(`SpeciesMap::Apply` + `TemporalInterpolator`), and the HEMCO aggregation — is
netCDF-free and ships to the browser as the genuine MIEM C++. A WASM-only
factory (`reader_factory_wasm.cpp`) hands the source an **in-memory reader**
fed those slices, so the unmodified pipeline runs each frame:

```
 on-mesh inventory .nc ─► miem_run ─┐
 mesh latCell/lonCell  ─► coords ───┼─► make_zarr.py ─► data/emissions.zarr ─┐
                                    │      (12 monthly slices + epochs)        │
 src/{emissions,source_offline,species_map,temporal_interpolator,…}.cpp        │
        + wasm_miem.cpp + reader_factory_wasm.cpp ─► emcc ─► miem_wasm.{js,wasm}│
                                                                              ▼
                                          index.html  (zarrita.js reads the store;
                                          miem_wasm runs Emissions::Run(t) per frame;
                                          canvas renders)
```

So hourly (or any) resolution needs no extra storage — the real
`Emissions::Run(t)` is evaluated on demand. The store holds every sector's
monthly slices (11 sectors × 12 months ≈ 85 MB here), and the **field** menu
reconfigures the `SpeciesMap` live: aggregate sectors (many→one), pass one
through, or split by mass fraction — the genuine `SpeciesMap::Apply`.
`held`/`nearest`/`linear` are the real `TemporalInterpolation` modes. The only
thing left offline is reading the file.

| file | role |
| --- | --- |
| `miem_run.cpp` | local driver: runs the real `Emissions` pipeline, extracts the masked monthly slices |
| `wasm_miem.cpp` | bridge: builds a real `Emissions` module and runs `Emissions::Run(t)` per frame |
| `wasm_in_memory_reader.hpp` | `EmissionFileReader` that serves the in-memory slices (no netCDF) |
| `reader_factory_wasm.cpp` | WASM-only `MakeEmissionFileReader` → the in-memory reader (replaces the netCDF factory) |
| `make_zarr.py` | joins slices + epochs + cell coordinates into a Zarr v3 store |
| `index.html` | static viewer: zarrita.js + canvas + the WASM pipeline |
| `build_and_run.sh` | compile driver + WASM → ensure deps/coords → build store → serve |
| `data/mesh_coords.npz` | cached cell lat/lon/area (re-runs skip the 214 MB mesh) |
| `data/land.geojson` | coarse coastline for map context (optional) |

## Why a separate mesh was needed

The on-mesh emission files carry only `(Time, nCells)` flux and `xtime` — **no
coordinates**. MIEM never needs them (the flux is already remapped), but a map
does. Cell `latCell`/`lonCell` live in the MPAS mesh file for the `x1.163842`
grid, whose cell ordering matches the inventory's `nCells` axis. We read the
mesh **once** at build time and cache the coordinates; the store is then
self-contained.

## Pointing it at other data

```sh
INPUT=/path/to/other_on_mesh.nc VAR=co_anth_sum SPECIES=CO ./viz/build_and_run.sh
```

If the new file is on a different mesh, delete `data/mesh_coords.npz` and set
`MESH=/path/to/that.grid.nc` (or let it download the standard `x1.163842`).

Note: a *scaling factor* would be a poorer demo than interpolation — MIEM's
`SpeciesMap` rejects per-species factors summing to >1 (`SCALING_EXCEEDS_ONE`),
so scaling is for mass-conserving species splitting, not amplification.

## Requirements

- A configured MIEM build (`build/src/libmiem.a`); the script builds the `miem`
  target if missing.
- A C++20 compiler + `nc-config` on `PATH` (Homebrew/conda netCDF) for the driver.
- **emscripten** (`emcc`) for the WASM interpolator (`brew install emscripten`).
- Python with `zarr>=3`, `netCDF4`, `numpy`; the script makes `viz/.venv` if the
  active Python lacks them.
