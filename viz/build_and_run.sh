#!/usr/bin/env bash
# Copyright (C) 2026 University Corporation for Atmospheric Research
# SPDX-License-Identifier: Apache-2.0
#
# One-shot build + serve for the MIEM emissions visualizer.
#
#   1. compile the local miem_run driver against the prebuilt libmiem.a
#   1b. compile the real miem::TemporalInterpolator to WASM (needs emcc)
#   2. ensure a Python with the zarr v3 stack (uses $PYTHON, else makes viz/.venv)
#   3. ensure cell coordinates (viz/data/mesh_coords.npz; derive from $MESH or
#      download the standard x1.163842 MPAS mesh if the cache is absent)
#   4. run make_zarr.py to (re)build viz/data/emissions.zarr (12 monthly slices)
#   5. serve viz/ over HTTP so index.html can read the store + run MIEM in WASM
#
# The store is self-contained once built: to *just view* an existing build,
# run `./viz/build_and_run.sh serve`.
#
# Overridable via env: INPUT, MESH, VAR, SPECIES, PORT, PYTHON, REPO.
set -euo pipefail

VIZ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$VIZ_DIR"

PORT="${PORT:-8000}"
DATA="$VIZ_DIR/data"
STORE="$DATA/emissions.zarr"
COORDS="$DATA/mesh_coords.npz"
mkdir -p "$DATA"

# --- python with the zarr v3 stack -----------------------------------------
# zarr>=3 needs Python >=3.11, so if the active interpreter can't satisfy the
# stack we build viz/.venv from the newest 3.11+ python we can find.
ensure_python() {
  PY="${PYTHON:-python3}"
  local stack='import zarr, netCDF4, numpy, scipy'
  if "$PY" -c "$stack" >/dev/null 2>&1; then return; fi
  if [ -x "$VIZ_DIR/.venv/bin/python" ] && "$VIZ_DIR/.venv/bin/python" -c "$stack" >/dev/null 2>&1; then
    PY="$VIZ_DIR/.venv/bin/python"; return
  fi
  echo "==> setting up viz/.venv (zarr v3 + netCDF4 + numpy + scipy)"
  local base="$PY"
  for c in "$PY" python3.13 python3.12 python3.11; do
    if command -v "$c" >/dev/null 2>&1 && \
       "$c" -c 'import sys; raise SystemExit(0 if sys.version_info[:2] >= (3,11) else 1)' 2>/dev/null; then
      base="$c"; break
    fi
  done
  "$base" -m venv "$VIZ_DIR/.venv"
  PY="$VIZ_DIR/.venv/bin/python"
  "$PY" -m pip install -q --upgrade pip
  "$PY" -m pip install -q "zarr>=3" numpy scipy netCDF4
}

# --- serve-only shortcut ----------------------------------------------------
if [ "${1:-}" = "serve" ]; then
  [ -d "$STORE" ] || { echo "no store at $STORE -- run without 'serve' to build first"; exit 1; }
  ensure_python
  echo "==> serving $VIZ_DIR at http://localhost:$PORT/"
  exec "$PY" -m http.server -d "$VIZ_DIR" "$PORT"
fi

# --- locate the MIEM checkout that owns the prebuilt library ----------------
REPO="${REPO:-$(cd "$(git rev-parse --git-common-dir)/.." && pwd)}"
LIB="$REPO/build/src/libmiem.a"
INCLUDE="$REPO/include"
if [ ! -f "$LIB" ]; then
  echo "==> libmiem.a not found; configuring + building the miem target"
  cmake -S "$REPO" -B "$REPO/build" >/dev/null
  cmake --build "$REPO/build" --target miem
fi

# --- 1. compile the driver (extracts the masked monthly slices, needs netCDF)-
if [ ! -x "$VIZ_DIR/miem_run" ] || [ "$VIZ_DIR/miem_run.cpp" -nt "$VIZ_DIR/miem_run" ]; then
  echo "==> compiling miem_run"
  c++ -std=c++20 -O2 -DMIEM_USE_DOUBLE -I"$INCLUDE" $(nc-config --cflags) \
    "$VIZ_DIR/miem_run.cpp" "$LIB" $(nc-config --libs) -o "$VIZ_DIR/miem_run"
fi

# --- 1b. compile the full MIEM surface-flux pipeline to WASM (no netCDF) -----
# Everything Emissions::Run touches except the netCDF readers: the WASM factory
# (reader_factory_wasm.cpp) feeds an in-memory reader instead, so the genuine
# builder/source/SpeciesMap/interpolator/aggregation all run in the browser.
WASM_SRCS="$REPO/src/emissions.cpp $REPO/src/emissions_builder.cpp $REPO/src/emissions_state.cpp \
$REPO/src/source_offline.cpp $REPO/src/source_factory.cpp $REPO/src/species_map.cpp \
$REPO/src/temporal_interpolator.cpp $REPO/src/flux_converter.cpp"
if [ ! -f "$VIZ_DIR/miem_wasm.js" ] || [ "$VIZ_DIR/wasm_miem.cpp" -nt "$VIZ_DIR/miem_wasm.js" ] \
   || [ "$VIZ_DIR/reader_factory_wasm.cpp" -nt "$VIZ_DIR/miem_wasm.js" ] \
   || [ "$REPO/src/emissions.cpp" -nt "$VIZ_DIR/miem_wasm.js" ]; then
  if command -v emcc >/dev/null 2>&1; then
    echo "==> compiling miem_wasm (full Emissions pipeline -> WASM)"
    emcc "$VIZ_DIR/wasm_miem.cpp" "$VIZ_DIR/reader_factory_wasm.cpp" $WASM_SRCS \
      -I"$INCLUDE" -I"$REPO/src" -I"$VIZ_DIR" -DMIEM_USE_DOUBLE -std=c++20 -O2 -fexceptions \
      -sMODULARIZE -sEXPORT_ES6 -sEXPORT_NAME=createMiem \
      -sEXPORTED_FUNCTIONS=_miem_load_times,_miem_load_sector,_miem_build,_miem_run_at,_malloc,_free \
      -sEXPORTED_RUNTIME_METHODS=HEAPF64 -sALLOW_MEMORY_GROWTH \
      -o "$VIZ_DIR/miem_wasm.js"
  else
    echo "!! emcc not found -- install emscripten (brew install emscripten) to build the WASM pipeline." >&2
    [ -f "$VIZ_DIR/miem_wasm.js" ] || { echo "   no prebuilt miem_wasm.js; the viewer cannot run."; exit 1; }
  fi
fi

# --- 2. python --------------------------------------------------------------
ensure_python

# --- 3. cell coordinates ----------------------------------------------------
if [ ! -f "$COORDS" ]; then
  if [ -z "${MESH:-}" ]; then
    MESH="$DATA/x1.163842.grid.nc"
    if [ ! -f "$MESH" ]; then
      echo "==> downloading standard x1.163842 MPAS mesh (~100 MB) for coordinates"
      curl -L --fail -o "$DATA/x1.163842.tar.gz" \
        https://www2.mmm.ucar.edu/projects/mpas/atmosphere_meshes/x1.163842.tar.gz
      tar xzf "$DATA/x1.163842.tar.gz" -C "$DATA" x1.163842.grid.nc
    fi
  fi
  echo "==> caching coordinates from $(basename "$MESH") -> $(basename "$COORDS")"
  "$PY" - "$MESH" "$COORDS" <<'PY'
import sys, netCDF4, numpy as np
mesh, out = sys.argv[1], sys.argv[2]
ds = netCDF4.Dataset(mesh)
lat = np.degrees(np.asarray(ds.variables["latCell"][:], dtype=np.float64)).astype(np.float32)
lon = np.degrees(np.asarray(ds.variables["lonCell"][:], dtype=np.float64))
area = np.asarray(ds.variables["areaCell"][:], dtype=np.float64)
sr = float(getattr(ds, "sphere_radius", 1.0))
area_m2 = area * (6371229.0 / sr) ** 2          # unit-sphere solid angle -> m^2
lon = ((lon + 180.0) % 360.0) - 180.0           # [0,360) -> [-180,180)
np.savez(out, lat=lat, lon=lon.astype(np.float32), area_m2=area_m2)
print("   cached", len(lat), "cells")
PY
fi

# --- 3b. cell polygon geometry for the WebGPU "actual cells" view (cells.html)
# Reconstruct the MPAS Voronoi cells from the cached centers (an SCVT mesh is the
# Voronoi diagram of its generators) and fan-triangulate them for the GPU.
GEOM="$DATA/mesh_geometry.zarr"
LAND="$DATA/land.geojson"
LAND_ARG=(); [ -f "$LAND" ] && LAND_ARG=(--land "$LAND")   # per-cell land tint (optional)
if [ ! -d "$GEOM" ] || [ "$VIZ_DIR/make_mesh_geometry.py" -nt "$GEOM/zarr.json" ] \
   || { [ -f "$LAND" ] && [ "$LAND" -nt "$GEOM/zarr.json" ]; }; then
  echo "==> building cell geometry store (spherical Voronoi -> triangles)"
  "$PY" "$VIZ_DIR/make_mesh_geometry.py" --coords "$COORDS" --out "$GEOM" "${LAND_ARG[@]}"
fi

# --- 4. build the store -----------------------------------------------------
INPUT="${INPUT:-/Users/vweeks/NCAR/ACOM/CheMPAS/sample_data/example_uptempo_data/CAMS-GLOB-ANT_2012_MPAS.x1.163842.grid.bc_c20260508.nc}"
echo "==> building Zarr store from $(basename "$INPUT")"
"$PY" "$VIZ_DIR/make_zarr.py" \
  --input "$INPUT" \
  --coords "$COORDS" \
  --runner "$VIZ_DIR/miem_run" \
  --out "$STORE" \
  --var "${VAR:-bc_anth_sum}" \
  --species "${SPECIES:-BC}" \
  --tmp "${TMPDIR:-/tmp}"

# --- 5. serve ---------------------------------------------------------------
echo "==> serving $VIZ_DIR at http://localhost:$PORT/   (Ctrl-C to stop)"
exec "$PY" -m http.server -d "$VIZ_DIR" "$PORT"
