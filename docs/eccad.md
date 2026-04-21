# ECCAD Convention for MIEM Input Files

## Overview

ECCAD is the NetCDF convention MIEM consumes for offline emission inventories.
The name follows GEIA/AERIS's **E**missions of atmospheric **C**ompounds and
**C**ompilation of **A**ncillary **D**ata project, whose distributed inventories
MIEM targets as its primary input format.

Files conforming to this schema are processed without a `DatasetDescriptor`;
non-conforming files require a descriptor to map their layout to ECCAD
conventions. Input paths are resolved via the inventory registry in the
top-level `miem.yaml` (see `docs/miem-config-1.0.md` once authored, and
the configs in `MIEM-config-schema/configs/example/`).

This spec replaces the MUSICA-internal SES 1.0 draft.

## Dimensions

| Dimension | Meaning | Notes |
|-----------|---------|-------|
| `n_cells` | Spatial cells (host-grid-aligned, 1D) | Required |
| `time` | Time steps | Required, unlimited |

## Global Attributes

| Attribute | Type | Required | Example |
|-----------|------|----------|---------|
| `eccad_version` | string | **Yes** | `"1.0"` |
| `Conventions` | string | **Yes** | `"CF-1.8"` |
| `grid_description` | string | Recommended | `"CAM-SE ne30np4"` |
| `source_inventory` | string | Recommended | `"CEDSv2024-04"` |
| `preprocessing_tool_version` | string | Recommended | `"UPTEMPO 0.1.0"` |

The `eccad_version` attribute is MIEM's detection key — readers peek at one
file per inventory at configure time and refuse to start if the attribute is
absent, pointing the user to this spec.

## Time Coordinate

- Variable name: `time`
- Must have a `units` attribute in CF format: e.g., `"days since 2000-01-01"`
- Calendar attribute recommended: `calendar = "standard"`

## Emission Variables

- Naming: `emi_<species>(time, n_cells)` — e.g., `emi_NOx`, `emi_SO2`
- Required `units` attribute: `"kg m-2 s-1"`
- Optional `_FillValue` attribute (masked to 0.0 by reader)
- Compression recommendation: `deflate_level=1`

## Example (CDL)

```
netcdf example_eccad {
dimensions:
    n_cells = 48602 ;
    time = UNLIMITED ;
variables:
    double time(time) ;
        time:units = "days since 2000-01-01" ;
        time:calendar = "standard" ;
    double emi_NOx(time, n_cells) ;
        emi_NOx:units = "kg m-2 s-1" ;
    double emi_SO2(time, n_cells) ;
        emi_SO2:units = "kg m-2 s-1" ;

// global attributes:
    :eccad_version = "1.0" ;
    :Conventions = "CF-1.8" ;
    :source_inventory = "CEDSv2024-04" ;
    :grid_description = "CAM-SE ne30np4" ;
}
```
