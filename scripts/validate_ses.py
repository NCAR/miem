#!/usr/bin/env python3
"""Validate that a NetCDF file conforms to SES (Standardized Emissions Schema).

Supports SES 1.0 and legacy CES (miem_version) files with backward compatibility.
"""

import sys

try:
    from netCDF4 import Dataset
except ImportError:
    print("netCDF4 not installed. Install with: pip install netCDF4")
    sys.exit(1)


def validate_ses(filepath):
    """Check SES compliance and report issues."""
    issues = []
    warnings = []

    try:
        ds = Dataset(filepath, "r")
    except Exception as e:
        print(f"FAIL: Cannot open file: {e}")
        return False

    attrs = ds.ncattrs()

    # Detect version attribute
    ses_version = None
    if "ses_version" in attrs:
        ses_version = ds.ses_version
    elif "miem_version" in attrs:
        ses_version = ds.miem_version
        warnings.append(
            "Using legacy 'miem_version' attribute — "
            "consider migrating to 'ses_version'"
        )
    else:
        issues.append(
            "Missing version attribute: expected 'ses_version' "
            "(or legacy 'miem_version')"
        )

    # Check Conventions attribute
    if "Conventions" not in attrs:
        warnings.append("Missing recommended attribute: Conventions (e.g., 'CF-1.8')")

    # Check dimensions — accept SES or legacy names
    has_time = "time" in ds.dimensions or "Time" in ds.dimensions
    has_cells = "n_cells" in ds.dimensions or "nCells" in ds.dimensions

    if not has_time:
        issues.append("Missing dimension: 'time' (or legacy 'Time')")
    elif "Time" in ds.dimensions and "time" not in ds.dimensions:
        warnings.append(
            "Using legacy dimension 'Time' — consider migrating to 'time'"
        )

    if not has_cells:
        issues.append("Missing dimension: 'n_cells' (or legacy 'nCells')")
    elif "nCells" in ds.dimensions and "n_cells" not in ds.dimensions:
        warnings.append(
            "Using legacy dimension 'nCells' — consider migrating to 'n_cells'"
        )

    # Check emission variables
    emi_vars = [v for v in ds.variables if v.startswith("emi_")]
    if not emi_vars:
        issues.append("No emission variables found (expected emi_* prefix)")

    # Determine expected dimension names
    time_dim = "time" if "time" in ds.dimensions else "Time"
    cell_dim = "n_cells" if "n_cells" in ds.dimensions else "nCells"

    for var_name in emi_vars:
        var = ds.variables[var_name]
        dims = var.dimensions

        expected_dims = (time_dim, cell_dim)
        if dims != expected_dims:
            issues.append(
                f"Variable {var_name}: expected dimensions {expected_dims}, "
                f"got {dims}"
            )

        if "units" not in var.ncattrs():
            warnings.append(f"Variable {var_name}: missing 'units' attribute")
        elif var.units != "kg m-2 s-1":
            warnings.append(
                f"Variable {var_name}: units='{var.units}', "
                f"expected 'kg m-2 s-1'"
            )

    ds.close()

    # Report
    compliance = "SES 1.0" if ses_version else "unknown"
    print(f"\nSES Validation: {filepath}")
    print(f"  Detected version: {ses_version or 'none'} ({compliance})")
    print(f"  Emission variables found: {len(emi_vars)}")
    for v in emi_vars:
        print(f"    - {v}")

    if warnings:
        print(f"\n  Warnings ({len(warnings)}):")
        for w in warnings:
            print(f"    ! {w}")

    if issues:
        print(f"\n  FAIL — {len(issues)} issue(s):")
        for issue in issues:
            print(f"    X {issue}")
        return False
    else:
        print("\n  PASS — File is SES-compliant")
        return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <netcdf_file>")
        sys.exit(1)

    success = validate_ses(sys.argv[1])
    sys.exit(0 if success else 1)
