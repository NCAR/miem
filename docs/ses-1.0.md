# Standardized Emissions Schema (SES) 1.0

## Overview

SES defines the NetCDF convention for emission input files consumed by MIEM.
Files conforming to this schema are processed without a DatasetDescriptor;
non-conforming files require a descriptor to map their layout to SES conventions.

## Dimensions

| Dimension | Meaning | Notes |
|-----------|---------|-------|
| `n_cells` | Spatial cells (host-grid-aligned, 1D) | Required |
| `time` | Time steps | Required, unlimited |

Legacy files using `nCells` and `Time` are accepted for backward compatibility.

## Global Attributes

| Attribute | Type | Required | Example |
|-----------|------|----------|---------|
| `ses_version` | string | **Yes** | `"1.0"` |
| `Conventions` | string | **Yes** | `"CF-1.8"` |
| `grid_description` | string | Recommended | `"CAM-SE ne30np4"` |
| `source_inventory` | string | Recommended | `"CEDSv2024-04"` |
| `preprocessing_tool_version` | string | Recommended | `"UPTEMPO 0.1.0"` |

Legacy files using `miem_version` instead of `ses_version` are accepted.

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
netcdf example_ses {
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
    :ses_version = "1.0" ;
    :Conventions = "CF-1.8" ;
    :source_inventory = "CEDSv2024-04" ;
    :grid_description = "CAM-SE ne30np4" ;
}
```

## Backward Compatibility

MIEM accepts legacy files that use:
- `miem_version` instead of `ses_version`
- `nCells` instead of `n_cells`
- `Time` instead of `time`

These files are detected as CES-compliant and processed identically to SES 1.0
files. New files should use SES 1.0 conventions.
