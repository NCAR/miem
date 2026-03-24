#!/usr/bin/env python3
"""Validate that a NetCDF file conforms to SES (Standardized Emissions Schema) 1.0."""

import sys

try:
    from netCDF4 import Dataset
except ImportError:
    print("netCDF4 not installed. Install with: pip install netCDF4")
    sys.exit(1)


def validate_ses(filepath):
    """Check SES 1.0 compliance and report issues."""
    issues = []
    warnings = []

    try:
        ds = Dataset(filepath, "r")
    except Exception as e:
        print(f"FAIL: Cannot open file: {e}")
        return False

    attrs = ds.ncattrs()

    # Check ses_version attribute
    ses_version = None
    if "ses_version" in attrs:
        ses_version = ds.ses_version
    else:
        issues.append("Missing required global attribute: 'ses_version'")

    # Check Conventions attribute
    if "Conventions" not in attrs:
        warnings.append("Missing recommended attribute: Conventions (e.g., 'CF-1.8')")

    # Check dimensions
    if "time" not in ds.dimensions:
        issues.append("Missing required dimension: 'time'")

    if "n_cells" not in ds.dimensions:
        issues.append("Missing required dimension: 'n_cells'")

    # Check emission variables
    emi_vars = [v for v in ds.variables if v.startswith("emi_")]
    if not emi_vars:
        issues.append("No emission variables found (expected emi_* prefix)")

    for var_name in emi_vars:
        var = ds.variables[var_name]
        dims = var.dimensions

        expected_dims = ("time", "n_cells")
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
    print(f"\nSES Validation: {filepath}")
    print(f"  Detected version: {ses_version or 'none'}")
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
        print("\n  PASS — File is SES 1.0 compliant")
        return True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <netcdf_file>")
        sys.exit(1)

    success = validate_ses(sys.argv[1])
    sys.exit(0 if success else 1)
