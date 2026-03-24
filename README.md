# MIEM — Model Independent Emissions Module

A standalone MUSICA science module for atmospheric emissions processing. MIEM provides a host-model-independent emissions interface that reads pre-regridded NetCDF emissions data, normalizes it through a configurable transformation layer, and provides a standard API for any host model (MPAS-A, CAM-SIMA, CATChem) to consume emissions.

## Architecture

MIEM follows the three-layer MUSICA pattern:

1. **C++ Core** — All emissions logic (reading, species mapping, temporal interpolation, flux conversion)
2. **C API** — `extern "C"` wrappers with opaque pointers for language interoperability
3. **Fortran Bindings** — `iso_c_binding` wrappers exposing idiomatic Fortran derived types

## Building

```bash
mkdir build && cd build
cmake .. -DMIEM_BUILD_FORTRAN=ON -DMIEM_BUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
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
- NetCDF-C
- yaml-cpp (fetched automatically)
- GoogleTest (fetched automatically)
- Fortran compiler (if `MIEM_BUILD_FORTRAN=ON`)

## Configuration

MIEM uses YAML configuration files. See `configs/` for examples.

## License

Apache 2.0. See [LICENSE](LICENSE).
