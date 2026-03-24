# MIEM Design Philosophy

## Why MIEM Exists

Atmospheric chemistry models need emissions — the injection of chemical species into the atmosphere from anthropogenic, fire, biogenic, and other sources. Currently, each host model implements its own emissions reading and processing logic, tightly coupled to specific file formats and model internals.

MIEM decouples emissions processing from the host model. Any host model that can call a C or Fortran API can consume emissions through MIEM without knowing anything about file formats, species naming conventions, or temporal interpolation strategies.

## Design Principles

1. **Host-model independence.** MIEM knows nothing about MPAS meshes, CAM grids, or any specific model's data structures. It operates on flat arrays of grid cells. The host provides air density and layer thickness; MIEM returns surface fluxes and tendencies.

2. **Configuration over code.** Adding a new emissions dataset requires writing a YAML descriptor, not modifying C++ source. Species mappings, unit conversions, and temporal interpolation modes are all configuration.

3. **Pre-regridded input.** MIEM assumes input data is already on the host model's grid (via UPTEMPO or equivalent preprocessing). Spatial regridding is explicitly out of scope for v1.

4. **Transformation layer.** The core architectural piece is a pipeline: raw NetCDF → CES validation/descriptor adaptation → unit normalization → species mapping → temporal interpolation → sector aggregation → flux-to-tendency conversion → EmisState output.

5. **Three-layer API.** Following the MICM pattern: C++ core for logic, C API for interoperability, Fortran bindings for host models. Each layer is independently testable.

6. **Extensibility through subclassing.** New emission source types (dust, sea salt, online biogenic) are added by subclassing `EmissionSource`, not by modifying existing code.
