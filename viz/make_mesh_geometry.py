#!/usr/bin/env python3
# Copyright (C) 2026 University Corporation for Atmospheric Research
# SPDX-License-Identifier: Apache-2.0
"""Triangulated MPAS cell geometry for the WebGPU "actual cells" renderer.

An MPAS mesh is the spherical centroidal Voronoi tessellation (SCVT) of its cell
centers, so the exact cell polygons can be reconstructed from the centers alone
-- no mesh-connectivity file needed -- via scipy's SphericalVoronoi. Each Voronoi
cell is fan-triangulated from its center, and the result is written as a Zarr v3
store the browser uploads to the GPU once:

  positions   (Nv, 3) float32  unit-sphere xyz of every triangle vertex
  cell_index  (Nv,)   uint32    which cell each vertex belongs to (-> value lookup)
  indices     (Nt*3,) uint32    triangle vertex indices (per-cell fans)

Geometry is per-cell -- no vertices are shared between cells -- so flat per-cell
coloring is unambiguous. Reproducible from the committed mesh_coords.npz: the
cell ordering of `cell_index` matches the inventory's nCells axis, so the browser
can color each cell directly by MIEM's per-cell flux.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import zarr
from scipy.spatial import SphericalVoronoi


def lonlat_to_xyz(lon_deg, lat_deg):
    """(lon, lat) in degrees -> unit-sphere cartesian, in cell order."""
    lon = np.radians(lon_deg.astype(np.float64))
    lat = np.radians(lat_deg.astype(np.float64))
    cos_lat = np.cos(lat)
    return np.stack([cos_lat * np.cos(lon), cos_lat * np.sin(lon), np.sin(lat)], axis=1)


def _lonlat(p):
    """Unit-sphere xyz (...,3) -> (lon, lat) in degrees."""
    lon = np.degrees(np.arctan2(p[..., 1], p[..., 0]))
    lat = np.degrees(np.arcsin(np.clip(p[..., 2], -1.0, 1.0)))
    return lon, lat


def land_mask_for(lon, lat, geojson_path):
    """Per-cell land/ocean bit (1 = center over land) from a GeoJSON coastline.

    Even-odd ray casting of each cell center against every ring (outer rings and
    holes alike), so lakes/inland seas punched as holes come out as ocean. The
    Natural Earth land polygons are split at the antimeridian, so a horizontal
    +lon ray never has to wrap. Returns a uint32 array in cell order, matching
    the inventory's nCells axis. The "actual cells" view tints no-data cells
    lighter where this is set, mirroring the pixel view's filled continents.
    """
    gj = json.loads(Path(geojson_path).read_text())
    rings = []
    for feat in gj.get("features", []):
        g = feat.get("geometry") or {}
        polys = (
            [g["coordinates"]] if g.get("type") == "Polygon"
            else g["coordinates"] if g.get("type") == "MultiPolygon"
            else []
        )
        for poly in polys:
            for ring in poly:
                r = np.asarray(ring, dtype=np.float64)
                if len(r) >= 4:
                    rings.append(r)

    px = lon.astype(np.float64)
    py = lat.astype(np.float64)
    inside = np.zeros(len(px), dtype=bool)
    for r in rings:                                  # accumulate crossing parity
        x, y = r[:, 0], r[:, 1]
        x1, y1, x2, y2 = x[:-1], y[:-1], x[1:], y[1:]
        for k in range(len(x1)):                     # vectorized over points per edge
            straddle = (y1[k] > py) != (y2[k] > py)  # ray at py crosses this edge's y-span
            dy = y2[k] - y1[k]
            xint = x1[k] + (py - y1[k]) * (x2[k] - x1[k]) / (dy if dy else 1.0)
            inside ^= straddle & (px < xint)
    return inside.astype(np.uint32)


def build_geometry(centers):
    """Voronoi-tessellate `centers` (N,3 unit sphere) and fan-triangulate.

    Returns positions (Nv,3 f32), cell_index (Nv u32), indices (Nt*3 u32),
    positions_xy (Nv,2 f32) and the per-cell polygon sizes. Cells are grouped by
    polygon size so the fans are assembled with vectorized numpy.

    positions_xy is the equirectangular (lon, lat) in degrees for the 2D map.
    Each cell's vertices are seam-unwrapped to the same 360° branch as the cell
    center, so no triangle spans the antimeridian; polar cells (which genuinely
    wrap most of a longitude circle) are collapsed to a degenerate point.
    """
    sv = SphericalVoronoi(centers, radius=1.0, center=np.zeros(3))
    sv.sort_vertices_of_regions()  # order each region's vertices around the cell
    verts = sv.vertices            # (nVor, 3) the Voronoi (cell-corner) vertices
    sizes = np.array([len(r) for r in sv.regions])

    pos_blocks, cid_blocks, idx_blocks, xy_blocks = [], [], [], []
    offset, n_polar = 0, 0
    for edges in np.unique(sizes):
        cells = np.where(sizes == edges)[0]
        m = len(cells)
        corner_ids = np.array([sv.regions[c] for c in cells], dtype=np.int64)  # (m, edges)
        ctr = centers[cells]                  # (m, 3)
        cor = verts[corner_ids]               # (m, edges, 3)
        # Per-cell vertex block: [center, corner_0 .. corner_{edges-1}] -> edges+1 verts.
        block = np.concatenate([ctr[:, None, :], cor], axis=1)
        pos_blocks.append(block.reshape(-1, 3).astype(np.float32))
        cid_blocks.append(np.repeat(cells.astype(np.uint32), edges + 1))

        # 2D equirectangular positions, unwrapped about each cell's center lon.
        clon, clat = _lonlat(ctr)
        vlon, vlat = _lonlat(cor)
        vlon = vlon + 360.0 * np.round((clon[:, None] - vlon) / 360.0)
        blon = np.concatenate([clon[:, None], vlon], axis=1)   # (m, edges+1)
        blat = np.concatenate([clat[:, None], vlat], axis=1)
        wrap = (blon.max(axis=1) - blon.min(axis=1)) > 90.0    # polar / un-drawable
        blon[wrap] = clon[wrap][:, None]
        blat[wrap] = clat[wrap][:, None]
        n_polar += int(wrap.sum())
        xy_blocks.append(np.stack([blon, blat], axis=-1).reshape(-1, 2).astype(np.float32))

        # Fan triangles (center, corner_i, corner_{i+1 mod edges}) in global vertex ids.
        base = offset + np.arange(m) * (edges + 1)
        i = np.arange(edges)
        i_next = (i + 1) % edges
        tri = np.stack(
            [
                np.repeat(base, edges),
                (base[:, None] + 1 + i[None, :]).reshape(-1),
                (base[:, None] + 1 + i_next[None, :]).reshape(-1),
            ],
            axis=1,
        ).astype(np.uint32)
        idx_blocks.append(tri.reshape(-1))
        offset += m * (edges + 1)

    build_geometry.n_polar_collapsed = n_polar
    return (
        np.concatenate(pos_blocks),
        np.concatenate(cid_blocks),
        np.concatenate(idx_blocks),
        np.concatenate(xy_blocks),
        sizes,
    )


def write_store(out_dir, positions, cell_index, indices, positions_xy, land_mask, meta):
    store = zarr.storage.LocalStore(str(out_dir))
    root = zarr.create_group(store=store, overwrite=True)

    def arr(name, data):
        a = root.create_array(name, shape=data.shape, chunks=data.shape, dtype=data.dtype, compressors=None)
        a[...] = data

    arr("positions", positions)        # unit-sphere xyz (globe); single chunk, ~13 MB
    arr("positions_xy", positions_xy)  # equirectangular lon/lat degrees (2D map)
    arr("cell_index", cell_index)
    arr("indices", indices)
    if land_mask is not None:
        arr("land_mask", land_mask)    # per-cell 1=land (tints no-data cells lighter)
    root.attrs.update(meta)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--coords", required=True, help="mesh_coords.npz (lat/lon in cell order)")
    ap.add_argument("--out", required=True, help="output Zarr v3 directory")
    ap.add_argument("--land", help="optional GeoJSON coastline -> per-cell land_mask")
    args = ap.parse_args()

    z = np.load(args.coords)
    lat, lon = z["lat"], z["lon"]
    n_cells = len(lat)
    print(f"[1/3] {n_cells} cell centers from {Path(args.coords).name}", flush=True)

    centers = lonlat_to_xyz(lon, lat)
    print("[2/3] spherical Voronoi + fan triangulation ...", flush=True)
    positions, cell_index, indices, positions_xy, sizes = build_geometry(centers)
    n_tri = len(indices) // 3

    land_mask = None
    if args.land and Path(args.land).exists():
        land_mask = land_mask_for(lon, lat, args.land)
        print(f"      land mask: {int(land_mask.sum()):,} / {n_cells:,} cells over land", flush=True)

    # Completeness check: the Voronoi cell areas must tile the whole sphere (4*pi).
    sphere_area = float(SphericalVoronoi(centers, radius=1.0, center=np.zeros(3)).calculate_areas().sum())

    hist = {int(e): int((sizes == e).sum()) for e in np.unique(sizes)}
    print(
        f"      {len(positions):,} tri-verts, {n_tri:,} triangles; "
        f"polygon sizes {hist}; Σarea={sphere_area:.4f} (4π={4*np.pi:.4f}); "
        f"{build_geometry.n_polar_collapsed} polar cells collapsed in 2D",
        flush=True,
    )

    print(f"[3/3] writing geometry store -> {args.out}", flush=True)
    write_store(
        Path(args.out),
        positions,
        cell_index,
        indices,
        positions_xy,
        land_mask,
        {
            "n_cells": int(n_cells),
            "n_vertices": int(len(positions)),
            "n_triangles": int(n_tri),
            "polygon_sizes": hist,
            "sphere_area_check": sphere_area,
            "polar_collapsed_2d": int(build_geometry.n_polar_collapsed),
            "land_cells": int(land_mask.sum()) if land_mask is not None else None,
            "source_coords": Path(args.coords).name,
            "geometry": "per-cell fans; positions=unit-sphere xyz, positions_xy=equirectangular lon/lat deg",
        },
    )
    extra = land_mask.nbytes if land_mask is not None else 0
    mb = (positions.nbytes + positions_xy.nbytes + cell_index.nbytes + indices.nbytes + extra) / 1e6
    print(f"      done ({mb:.1f} MB).", flush=True)


if __name__ == "__main__":
    main()
