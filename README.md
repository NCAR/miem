# MIEM — Model Independent Emissions Module

A standalone MUSICA science module for atmospheric emissions processing. MIEM provides a host-model-independent emissions interface that reads pre-regridded NetCDF emissions data, normalizes it through a configurable transformation layer, and provides a standard API for any host model (MPAS-A, CAM-SIMA, CATChem) to consume emissions.

## Architecture

MIEM follows the three-layer MUSICA pattern:

1. **C++ Core** — All emissions logic (reading, species mapping, temporal interpolation, flux conversion)
2. **C API** — `extern "C"` wrappers with opaque pointers for language interoperability
3. **Fortran Bindings** — `iso_c_binding` wrappers exposing idiomatic Fortran derived types

## Building

```bash
cmake -B build -DMIEM_BUILD_FORTRAN=ON -DMIEM_BUILD_TESTS=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

### NetCDF Discovery

CMake will auto-detect NetCDF-C in two ways:

1. **Standard installs** — via `netCDFConfig.cmake` (e.g., `apt install libnetcdf-dev`)
2. **Custom/HPC installs** — via `nc-config` on your `PATH` (e.g., `/opt/mpas/netcdf`)

If NetCDF is installed in a non-standard location and `nc-config` is not on your PATH, point CMake to it:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/netcdf
```

Or set the `NETCDF_DIR` environment variable:

```bash
export NETCDF_DIR=/opt/mpas/netcdf
cmake -B build
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MIEM_BUILD_FORTRAN` | `ON` | Build Fortran bindings |
| `MIEM_BUILD_TESTS` | `ON` | Build test suite |
| `MIEM_DOUBLE_PRECISION` | `ON` | Use 64-bit floats |

### Dependencies

- CMake >= 3.14
- C++17 compiler
- NetCDF-C (detected via `netCDFConfig.cmake` or `nc-config`)
- yaml-cpp (fetched automatically via CMake FetchContent)
- GoogleTest (fetched automatically via CMake FetchContent)
- Fortran compiler (if `MIEM_BUILD_FORTRAN=ON`)

## Configuration

MIEM uses YAML configuration files. See `configs/` for examples.

## License

Apache 2.0. See [LICENSE](LICENSE).
