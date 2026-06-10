# Test data

Small, real emission fixtures committed for MIEM's tests. Kept to a few MB so
they live in plain git (no LFS).

## `CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc` (932 KB)

A heavily-reduced but **real** slice of the CAMS-GLOB-ANT v6.2 black-carbon
emissions for 2012, in the MPAS-regridded ("UPTEMPO output") layout that MIEM's
`"eccad"`-convention reader consumes — i.e. flux laid out flat on the MPAS mesh
(`Time`, `nCells`), not the raw ECCAD lat/lon grid.

| | |
| --- | --- |
| Source | `CAMS-GLOB-ANT_2012_MPAS.x1.163842.grid.bc_c20260508.nc` (94 MB, Forrest Lacey & Rajesh Kumar) |
| Reduction | every 40th cell of the `x1.163842` mesh → **4097 of 163842 cells** |
| Retained | all 12 anthropogenic BC sectors (`bc_anth_awb…bc_anth_sum`), all 12 months, `xtime` |
| Values | unmodified — real `kg m-2 s-1` fluxes (provenance stamped in the `subset_*` global attributes) |

The cells are sampled by a global stride rather than a contiguous block: an MPAS
mesh is not spatially ordered, so striding keeps a worldwide land/ocean mix
(≈39k of the 49k cell-months carry nonzero emissions) instead of one local
patch.

### Why it's here before there's a reader

This file is committed by the `Source`-schema PR, which has no file reader yet —
it answers a reviewer request to have real data on hand and pre-positions the
fixture so the ECCAD-reader PR can wire a read test against it without
re-sourcing data.

### Regenerating

```sh
python3 -m pip install netCDF4 numpy
python3 tools/subset_eccad.py <full_MPAS_grid_file>.nc \
    test/data/CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc 40
```
