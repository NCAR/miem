#!/usr/bin/env python3
# Copyright (C) 2026 University Corporation for Atmospheric Research
# SPDX-License-Identifier: Apache-2.0
"""Build the Zarr v3 store the emissions visualizer reads.

The store holds the inputs the browser needs to animate MIEM's flux at any time
resolution -- the per-frame interpolation itself runs in the browser via the
WASM-compiled miem::TemporalInterpolator, so only the monthly slices ship:

  * slices  -- the 12 masked monthly flux slices MIEM reads (extracted through
               the real reader by miem_run, NaN/_FillValue -> 0)
  * epochs  -- the slice times [UTC s], so the browser can bracket any instant
  * lat/lon/area -- cell geometry, from the matching MPAS mesh (joined once here)

The on-mesh emission files carry no coordinates of their own, so the mesh is
needed once, at build time, to place each cell on a map. After this runs, the
store under --out is fully self-contained.

The store is written with no compression (bytes-only codec) so zarrita.js can
read it in the browser with zero codec dependencies; uncompressed it is ~10 MB.
"""
import argparse
import calendar
import subprocess
import sys
from pathlib import Path

import netCDF4
import numpy as np
import zarr


def parse_xtime(ds):
    """Return (labels, epoch_seconds) from an MPAS `xtime` char variable.

    Reproduces the reader's interpretation: "YYYY-MM-DD_HH:MM:SS" read as UTC
    on a proleptic-Gregorian calendar. `calendar.timegm` matches MIEM's
    std::chrono::sys_days path exactly (2012-01-01 -> 1325376000).
    """
    raw = netCDF4.chartostring(ds.variables["xtime"][:])
    labels, epochs = [], []
    for item in np.atleast_1d(raw):
        stamp = str(item).strip()
        date, _, clock = stamp.partition("_")
        year, month, day = (int(p) for p in date.split("-"))
        hour = minute = second = 0
        if clock:
            parts = clock.split(":")
            hour = int(parts[0])
            minute = int(parts[1]) if len(parts) > 1 else 0
            second = int(parts[2]) if len(parts) > 2 else 0
        labels.append(stamp)
        epochs.append(float(calendar.timegm((year, month, day, hour, minute, second, 0, 0, 0))))
    return labels, epochs


def read_inventory(input_path, var):
    ds = netCDF4.Dataset(input_path, "r")
    try:
        labels, epochs = parse_xtime(ds)
        # Keep the inventory's own fill/NaN as NaN so ocean cells read as
        # "no data" in the viewer, distinct from a genuine zero.
        field = ds.variables[var]
        field.set_auto_mask(False)
        data = np.asarray(field[:], dtype=np.float32)  # (Time, nCells)
    finally:
        ds.close()
    return labels, epochs, data


# Earth radius MPAS uses for its quasi-uniform meshes (the "a" sphere radius).
EARTH_RADIUS_M = 6371229.0


def read_mesh(mesh_path, n_cells):
    ds = netCDF4.Dataset(mesh_path, "r")
    try:
        lat = np.degrees(np.asarray(ds.variables["latCell"][:], dtype=np.float64))
        lon = np.degrees(np.asarray(ds.variables["lonCell"][:], dtype=np.float64))
        area = np.asarray(ds.variables["areaCell"][:], dtype=np.float64)
        # Distributed MPAS meshes live on a unit sphere (sphere_radius = 1), so
        # areaCell is a solid angle (sums to 4*pi). Scale to m^2 so the global
        # totals come out in real kg/s; area scales with radius^2.
        sphere_radius = float(getattr(ds, "sphere_radius", 1.0))
    finally:
        ds.close()
    if len(lat) != n_cells:
        sys.exit(
            f"mesh nCells ({len(lat)}) != inventory nCells ({n_cells}); "
            "the mesh does not match this inventory file."
        )
    area_m2 = area * (EARTH_RADIUS_M / sphere_radius) ** 2
    # MPAS stores longitude in [0, 2pi); wrap to [-180, 180) for an
    # equirectangular map.
    lon = ((lon + 180.0) % 360.0) - 180.0
    return lat.astype(np.float32), lon.astype(np.float32), area_m2


def read_coords(coords_path, n_cells):
    """Load lat/lon (deg) and area (m^2) from the small npz cache, so a re-run
    does not need the full multi-hundred-MB mesh file again."""
    z = np.load(coords_path)
    lat, lon, area_m2 = z["lat"], z["lon"], z["area_m2"]
    if len(lat) != n_cells:
        sys.exit(
            f"coords cache nCells ({len(lat)}) != inventory nCells ({n_cells}); "
            "the cache does not match this inventory file."
        )
    return lat.astype(np.float32), lon.astype(np.float32), area_m2


def discover_sectors(input_path):
    """The individual sector variables: float (Time, nCells) fields, minus the
    char xtime and the precomputed *_sum total."""
    ds = netCDF4.Dataset(input_path, "r")
    try:
        dims_ok = ("Time", "nCells")
        names = [
            n
            for n, v in ds.variables.items()
            if v.dtype.kind == "f" and tuple(v.dimensions) == dims_ok and not n.endswith("_sum")
        ]
    finally:
        ds.close()
    if not names:
        sys.exit(f"no (Time, nCells) sector variables found in {input_path}")
    return names


def run_miem(runner, input_path, var, species, n_cells, interp, epochs, tmp_dir):
    out_bin = Path(tmp_dir) / f"miem_out_{var}_{interp}.bin"
    cmd = [
        str(runner),
        str(input_path),
        var,
        species,
        str(n_cells),
        interp,
        str(out_bin),
        *[f"{e:.1f}" for e in epochs],
    ]
    print(f"  $ miem_run … {interp} <{len(epochs)} epochs>", flush=True)
    subprocess.run(cmd, check=True)
    out = np.fromfile(out_bin, dtype=np.float64)
    return out.reshape(len(epochs), n_cells).astype(np.float32)


def color_range(*fields):
    """Robust log10 color limits over positive, finite values of all fields."""
    pos = []
    for f in fields:
        v = f[np.isfinite(f) & (f > 0)]
        if v.size:
            pos.append(v)
    if not pos:
        return -12.0, -6.0
    allpos = np.log10(np.concatenate(pos))
    vmax = float(np.percentile(allpos, 99.5))
    # Floor at p10, but never open the window wider than ~6 decades: the
    # faint sub-p10 tail is mostly numerical dust and would waste the ramp.
    vmin = max(float(np.percentile(allpos, 10.0)), vmax - 6.0)
    return vmin, vmax


def weighted_totals(field, area):
    """Per-time-step area-weighted global total [kg s-1] (NaN treated as 0)."""
    clean = np.nan_to_num(field, nan=0.0)
    return (clean * area[None, :]).sum(axis=1).astype(np.float64).tolist()


def write_store(out_dir, lon, lat, area, sector_slices, epochs, labels, meta):
    n_sec, n_time, n_cells = sector_slices.shape
    store = zarr.storage.LocalStore(str(out_dir))
    root = zarr.create_group(store=store, overwrite=True)

    def arr(name, data, chunks):
        a = root.create_array(name, shape=data.shape, chunks=chunks, dtype=data.dtype, compressors=None)
        a[...] = data
        return a

    arr("lon", lon, (n_cells,))
    arr("lat", lat, (n_cells,))
    arr("area", area.astype(np.float32), (n_cells,))
    # The masked monthly slices for every sector (one chunk per sector-month),
    # plus their UTC times so the WASM pipeline can bracket any requested
    # instant. The browser feeds these to the real SpeciesMap + interpolator.
    arr("sector_slices", sector_slices, (1, 1, n_cells))
    arr("epochs", np.asarray(epochs, dtype=np.float64), (n_time,))

    root.attrs.update(meta)
    root.attrs["time_labels"] = labels


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", required=True, help="UPTEMPO on-mesh emission NetCDF")
    ap.add_argument("--mesh", help="matching MPAS mesh (latCell/lonCell/areaCell)")
    ap.add_argument("--coords", help="npz coords cache (lat/lon/area_m2) -- alternative to --mesh")
    ap.add_argument("--runner", required=True, help="path to the compiled miem_run driver")
    ap.add_argument("--out", required=True, help="output Zarr v3 directory")
    ap.add_argument("--vars", default="", help="comma-separated inventory sectors (default: auto-discover)")
    ap.add_argument("--species", default="BC", help="mechanism species the sectors map onto")
    ap.add_argument("--tmp", default=".", help="scratch dir for the driver's binary output")
    args = ap.parse_args()

    sectors = [s.strip() for s in args.vars.split(",") if s.strip()] or discover_sectors(args.input)
    print(f"[1/4] reading {Path(args.input).name}: {len(sectors)} sectors {sectors} ...", flush=True)
    labels, epochs, first = read_inventory(args.input, sectors[0])
    n_time, n_cells = first.shape
    print(f"      {n_time} monthly slices x {n_cells} cells; {labels[0]} .. {labels[-1]}", flush=True)

    if args.coords:
        print(f"[2/4] reading coords cache {Path(args.coords).name} ...", flush=True)
        lat, lon, area = read_coords(args.coords, n_cells)
        coord_source = Path(args.coords).name
    elif args.mesh:
        print(f"[2/4] reading mesh {Path(args.mesh).name} ...", flush=True)
        lat, lon, area = read_mesh(args.mesh, n_cells)
        coord_source = Path(args.mesh).name
    else:
        sys.exit("need either --coords (npz cache) or --mesh (MPAS mesh NetCDF)")

    # Extract each sector's masked monthly slices through the real reader
    # (NaN/_FillValue -> 0). The browser feeds all sectors to MIEM's SpeciesMap
    # to aggregate/split, and the interpolator to bracket any instant.
    print(f"[3/4] extracting {len(sectors)} sectors x {n_time} months via MIEM ...", flush=True)
    sector_slices = np.empty((len(sectors), n_time, n_cells), dtype=np.float32)
    for k, v in enumerate(sectors):
        sector_slices[k] = run_miem(args.runner, args.input, v, v, n_cells, "nearest", epochs, args.tmp)

    print(f"[4/4] writing Zarr v3 store -> {args.out} ...", flush=True)
    aggregate = sector_slices.sum(axis=0)  # the Σ-all-sectors field sets the color range
    vmin, vmax = color_range(aggregate)
    sector_totals = [weighted_totals(sector_slices[k], area) for k in range(len(sectors))]
    meta = {
        "sectors": sectors,
        "mech_species": args.species,
        "units": "kg m-2 s-1",
        "n_sectors": int(len(sectors)),
        "n_time": int(n_time),
        "n_cells": int(n_cells),
        "log_vmin": vmin,
        "log_vmax": vmax,
        "sector_totals": sector_totals,  # [sector][month] area-weighted total [kg/s]
        "total_units": "kg s-1",
        "source_input": Path(args.input).name,
        "source_mesh": coord_source,
    }
    write_store(Path(args.out), lon, lat, area, sector_slices, epochs, labels, meta)

    agg_tot = np.array(weighted_totals(aggregate, area))
    print(
        f"      done. {len(sectors)} sectors x {n_time} months, log range [{vmin:.2f}, {vmax:.2f}]; "
        f"Σ-all global total {agg_tot.min():.1f}–{agg_tot.max():.1f} kg/s",
        flush=True,
    )


if __name__ == "__main__":
    main()
