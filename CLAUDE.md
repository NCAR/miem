# MIEM — Claude Code Project Config

## Project Overview

MIEM (Model Independent Emissions Module) is a C++ MUSICA science module for atmospheric emissions processing. Three-layer architecture: C++ core → C API → Fortran iso_c_binding.

## Build

```bash
cmake -B build -DMIEM_BUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

NetCDF-C is auto-detected via `nc-config` or `netCDFConfig.cmake`. For non-standard
installs: `cmake -B build -DCMAKE_PREFIX_PATH=/path/to/netcdf`

## Code Conventions

- C++17 standard
- PascalCase for class names and public methods (e.g., `EmissionsModule`, `QuerySpecies`)
- snake_case for local variables and private members
- Trailing underscore for private member variables (e.g., `n_cells_`)
- Header files in `include/miem/`, implementations in `src/`
- C API functions prefixed with `MIEM` (e.g., `CreateMIEM`, `MIEMRun`)
- Fortran types use `_t` suffix (e.g., `miem_t`, `emis_state_t`)
- Error handling: C++ exceptions in core, converted to error structs at C boundary
- All public headers use `#pragma once`
- Use `std::vector` for dynamic arrays, raw pointers only at C API boundary
- Configuration via yaml-cpp (`YAML::Node`)
- NetCDF I/O via NetCDF-C API

## Testing

- GoogleTest for C++ unit tests in `test/unit/`
- Fortran tests in `test/fortran/`
- Test data fixtures in `test/data/`

## Key Patterns

- Follow MICM's `HandleErrors` template for C API error handling
- `EmisState` provides `.data()` pointers for C/Fortran interop
- Species discovery via `QuerySpecies()` static method before construction
- Host index resolution via `ResolveHostIndices()` after construction

## SES (Standardized Emissions Schema)

- Input file convention defined in `docs/ses-1.0.md`
- SES 1.0 dimensions: `n_cells`, `time`
- SES 1.0 version attribute: `ses_version`
- SESReader detects SES compliance; non-SES files use DatasetDescriptor
- Category/hierarchy aggregation (HEMCO-style): categories sum, higher hierarchy wins within same category
- Per-source `scaling_factor` for runtime scenario perturbation
- Sector labels enable per-sector diagnostics via `EmisState::sector_fluxes`
