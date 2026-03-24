#!/usr/bin/env python3
"""Validate that a NetCDF file is CES (Canonical Emission Standard) compliant."""

import sys

try:
    from netCDF4 import Dataset
except ImportError:
    print("netCDF4 not installed. Install with: pip install netCDF4")
    sys.exit(1)


def validate_ces(filepath):
    """Check CES compliance and report issues."""
    issues = []
    warnings = []

    try:
        ds = Dataset(filepath, "r")
    except Exception as e:
        print(f"FAIL: Cannot open file: {e}")
        return False

    # Check global attributes
    if "miem_version" not in ds.ncattrs():
        issues.append("Missing global attribute: miem_version")

    # Check dimensions
    if "Time" not in ds.dimensions:
        issues.append("Missing dimension: Time")
    if "nCells" not in ds.dimensions:
        issues.append("Missing dimension: nCells")

    # Check emission variables
    emi_vars = [v for v in ds.variables if v.startswith("emi_")]
    if not emi_vars:
        issues.append("No emission variables found (expected emi_* prefix)")

    for var_name in emi_vars:
        var = ds.variables[var_name]
        dims = var.dimensions

        # Check dimensions
        expected_dims = ("Time", "nCells")
        if dims != expected_dims:
            issues.append(
                f"Variable {var_name}: expected dimensions {expected_dims}, "
                f"got {dims}"
            )

        # Check units attribute
        if "units" not in var.ncattrs():
            warnings.append(f"Variable {var_name}: missing 'units' attribute")
        elif var.units != "kg m-2 s-1":
            warnings.append(
                f"Variable {var_name}: units='{var.units}', "
                f"expected 'kg m-2 s-1'"
            )

    ds.close()

    # Report
    print(f"\nCES Validation: {filepath}")
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
        print("\n  PASS — File is CES-compliant")
        return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <netcdf_file>")
        sys.exit(1)

    success = validate_ces(sys.argv[1])
    sys.exit(0 if success else 1)
