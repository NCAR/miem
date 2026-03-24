#!/usr/bin/env python3
"""Generate test NetCDF files for MIEM unit tests.

Produces SES 1.0 compliant files using the Standardized Emissions Schema
conventions (ses_version, n_cells, time, CF-1.8).
"""

import numpy as np

try:
    from netCDF4 import Dataset
except ImportError:
    print("netCDF4 not installed. Install with: pip install netCDF4")
    exit(1)


def create_ses_compliant_file(filepath, n_cells=10, n_times=2):
    """Create an SES 1.0 compliant emissions file."""
    ds = Dataset(filepath, "w", format="NETCDF4")

    # Global attributes (SES 1.0 compliance)
    ds.ses_version = "1.0"
    ds.Conventions = "CF-1.8"
    ds.source_inventory = "TEST-INV-v1"
    ds.grid_description = "test_grid"

    # Dimensions (SES 1.0 names)
    ds.createDimension("time", n_times)
    ds.createDimension("n_cells", n_cells)

    # Time variable with CF-compliant units
    time_var = ds.createVariable("time", "f8", ("time",))
    time_var.units = "seconds since 1970-01-01"
    time_var.calendar = "standard"
    time_var[:] = np.arange(n_times, dtype=np.float64) * 86400.0  # Daily

    # Emission species
    species = {"NOx": 1e-9, "SO2": 5e-10, "CO": 2e-8}
    for name, base_flux in species.items():
        var = ds.createVariable(
            f"emi_{name}", "f8", ("time", "n_cells"), zlib=True, complevel=1
        )
        var.units = "kg m-2 s-1"
        for t in range(n_times):
            var[t, :] = base_flux * (1 + 0.1 * np.arange(n_cells)) * (1 + 0.05 * t)

    ds.close()
    print(f"Created SES 1.0 compliant file: {filepath}")


def create_non_ces_file(filepath, n_cells=10, n_times=2):
    """Create a non-SES file (requires descriptor)."""
    ds = Dataset(filepath, "w", format="NETCDF4")

    # No ses_version — not SES-compliant
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
    print(f"Created non-SES file: {filepath}")


if __name__ == "__main__":
    import os
    test_data_dir = os.path.join(os.path.dirname(__file__), "..", "test", "data")
    os.makedirs(test_data_dir, exist_ok=True)

    create_ses_compliant_file(os.path.join(test_data_dir, "test_anthro.nc"))
    create_non_ces_file(os.path.join(test_data_dir, "test_fire.nc"))
    print("Test data generation complete.")
