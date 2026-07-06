# Test data

Small, real emission fixtures committed for MIEM's tests. Kept to a few MB so
they live in plain git (no LFS).

## `CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc` (932 KB)

A heavily-reduced but **real** slice of the CAMS-GLOB-ANT v6.2 black-carbon
emissions for 2012, in the MPAS-regridded ("UPTEMPO output") layout that MIEM's
`"uptempo"`-convention reader consumes — i.e. flux laid out flat on the MPAS
mesh (`Time`, `nCells`), not the raw ECCAD lat/lon grid.

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

### How it's used

MIEM's `"uptempo"`-convention reader (`UptempoReader`) reads this file directly:
the `uptempo_reader` unit test checks variable discovery, `xtime` decoding, and
NaN-masked-cell handling against it, and the `bc_pipeline` integration test runs
the full pipeline (`bc_anth_sum` → `BC`) through to per-cell surface flux. The
fixture was first committed ahead of the reader, to answer a reviewer request
for real data on hand.

### Regenerating

```sh
python3 -m pip install netCDF4 numpy
python3 tools/subset_eccad.py <full_MPAS_grid_file>.nc \
    test/data/CAMS-GLOB-ANT_2012_MPAS_bc_subset.nc 40
```

## `x1.163842_2024_nox_subset.nc` (336 KB)

The same reduction applied to a **real** 2024 anthropogenic nitrogen-dioxide
(NOx) inventory, also in the MPAS-regridded on-mesh layout. Exists to give
MIEM's uptempo pipeline a second, differently-shaped real species (fewer
sectors, a different year) alongside the black-carbon fixture above.

| | |
| --- | --- |
| Source | `x1.163842-2024-anth_nitrogen-dioxide.MPAS.nc` (23 MB, Forrest Lacey & Rajesh Kumar) |
| Reduction | every 40th cell of the `x1.163842` mesh → **4097 of 163842 cells** |
| Retained | all 3 anthropogenic NOx sectors (`nox_anth_awb`, `nox_anth_res`, `nox_anth_sum`), all 12 months (2024), `xtime` |
| Values | unmodified — real `kg m-2 s-1` fluxes (≈41.5k of 49.2k cell-months nonzero; provenance stamped in the `subset_*` global attributes) |

### Regenerating

```sh
python3 -m pip install netCDF4 numpy
python3 tools/subset_eccad.py <full_MPAS_grid_file>.nc \
    test/data/x1.163842_2024_nox_subset.nc 40 "nitrogen dioxide (NOx)"
```
