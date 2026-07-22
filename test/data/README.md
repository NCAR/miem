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

## `x1.163842_2024_finn_subset.nc` (1.1 MB)

A **real** FINN v2.5.1 fire (biomass-burning) emissions inventory, also in the
MPAS-regridded on-mesh layout. Gives MIEM's uptempo pipeline a real `"fire"`
(rather than `"anthropogenic"`) source: a different `SourceType`, hourly
(rather than monthly) native cadence, and seven species with no overlap with
the BC/NOx fixtures above.

| | |
| --- | --- |
| Source | `FINNv2.5.1_modvrs_nrt_MOZART_2024_x1.163842.static_hourly_netcdf3.nc` (9.9 GB, Forrest Lacey & Rajesh Kumar), hourly 2024-10-14 through 2024-11-30 |
| Reduction | every 40th cell of the `x1.163842` mesh → **4097 of 163842 cells**; every 24th hourly timestep (daily 00:00 snapshots) → **48 of 1152 times** |
| Retained | all 7 biomass-burning species (`so2_biob_modis`, `mnt_biob_modis`, `nh3_biob_modis`, `co_biob_modis`, `bc_biob_modis`, `iso_biob_modis`, `oc_biob_modis`), `xtime` |
| Values | unmodified — real `kg m-2 s-1` fluxes (7008 of 196,656 cell-times nonzero per species — fire emissions are spatially sparse compared to the anthropogenic fixtures above; provenance stamped in the `subset_*` global attributes) |

### Regenerating

```sh
python3 -m pip install netCDF4 numpy
python3 tools/subset_eccad.py <full_FINN_MPAS_file>.nc \
    test/data/x1.163842_2024_finn_subset.nc 40 "FINN fire" 24
```

Uses the optional 5th `time_stride` argument (added for this fixture) since
the source file is hourly rather than monthly — a plain 40x cell stride alone
would keep all 1152 timesteps and produce a fixture over 20x larger than the
other two.
