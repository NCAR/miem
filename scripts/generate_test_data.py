#!/usr/bin/env python3
"""Generate test NetCDF files for MIEM unit tests."""

import numpy as np

try:
    from netCDF4 import Dataset
except ImportError:
    print("netCDF4 not installed. Install with: pip install netCDF4")
    exit(1)


def create_ces_compliant_file(filepath, n_cells=10, n_times=2):
    """Create a CES-compliant emissions file."""
    ds = Dataset(filepath, "w", format="NETCDF4")

    # Global attributes (CES compliance)
    ds.miem_version = "1.0"
    ds.emission_sector = "anthropogenic"
    ds.inventory_id = "TEST-INV-v1"

    # Dimensions
    ds.createDimension("Time", n_times)
    ds.createDimension("nCells", n_cells)

    # Time variable
    time_var = ds.createVariable("Time", "f8", ("Time",))
    time_var[:] = np.arange(n_times, dtype=np.float64) * 86400.0  # Daily

    # Emission species
    species = {"NOx": 1e-9, "SO2": 5e-10, "CO": 2e-8}
    for name, base_flux in species.items():
        var = ds.createVariable(f"emi_{name}", "f8", ("Time", "nCells"))
        var.units = "kg m-2 s-1"
        # Vary by cell and time
        for t in range(n_times):
            var[t, :] = base_flux * (1 + 0.1 * np.arange(n_cells)) * (1 + 0.05 * t)

    ds.close()
    print(f"Created CES-compliant file: {filepath}")


def create_non_ces_file(filepath, n_cells=10, n_times=2):
    """Create a non-CES file (requires descriptor)."""
    ds = Dataset(filepath, "w", format="NETCDF4")

    # No miem_version attribute — not CES-compliant
    ds.source = "FINN v2.5"

    # Dimensions (non-standard names)
    ds.createDimension("time", n_times)
    ds.createDimension("ncol", n_cells)

    # Time variable
    time_var = ds.createVariable("time", "f8", ("time",))
    time_var[:] = np.arange(n_times, dtype=np.float64) * 86400.0

    # Non-standard variable names
    species = {"fire_NOx": 1e15, "fire_CO": 5e16}
    for name, base_flux in species.items():
        var = ds.createVariable(name, "f8", ("time", "ncol"))
        var.units = "molecules cm-2 s-1"
        for t in range(n_times):
            var[t, :] = base_flux * (1 + 0.2 * np.arange(n_cells)) * (1 + 0.1 * t)

    ds.close()
    print(f"Created non-CES file: {filepath}")


if __name__ == "__main__":
    import os
    test_data_dir = os.path.join(os.path.dirname(__file__), "..", "test", "data")
    os.makedirs(test_data_dir, exist_ok=True)

    create_ces_compliant_file(os.path.join(test_data_dir, "test_anthro.nc"))
    create_non_ces_file(os.path.join(test_data_dir, "test_fire.nc"))
    print("Test data generation complete.")
