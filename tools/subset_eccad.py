#!/usr/bin/env python3
"""Produce a heavily-reduced *real* CAMS-GLOB-ANT emissions fixture.

Takes the MPAS-regridded UPTEMPO output (the flat-mesh layout MIEM's ECCAD
reader actually consumes) and keeps a global stride of cells -- real values,
all sectors, all 12 months -- shrunk to a couple of MB so it can live in-repo
as a reader test fixture.

Usage: subset_eccad.py <src.nc> <dst.nc> [stride] [species label]
"""
import sys
import numpy as np
import netCDF4 as nc

SRC = sys.argv[1]
DST = sys.argv[2]
STRIDE = int(sys.argv[3]) if len(sys.argv) > 3 else 40
SPECIES = sys.argv[4] if len(sys.argv) > 4 else "black carbon"

src = nc.Dataset(SRC, "r")
n_cells_full = len(src.dimensions["nCells"])
idx = np.arange(0, n_cells_full, STRIDE)  # global strided sample

dst = nc.Dataset(DST, "w", format="NETCDF4")

# Dimensions
dst.createDimension("Time", None)  # unlimited, matches source
dst.createDimension("nCells", len(idx))
dst.createDimension("StrLen", len(src.dimensions["StrLen"]))

def copy_attrs(src_var, dst_var):
    for a in src_var.ncattrs():
        if a == "_FillValue":
            continue  # set at creation time
        dst_var.setncattr(a, src_var.getncattr(a))

for name, var in src.variables.items():
    fill = var.getncattr("_FillValue") if "_FillValue" in var.ncattrs() else None
    if name == "xtime":
        out = dst.createVariable(name, var.dtype, var.dimensions)
        out[:] = var[:]
    else:  # <species>_anth_*(Time, nCells)
        out = dst.createVariable(
            name, var.dtype, var.dimensions,
            zlib=True, complevel=4, fill_value=fill,
        )
        out[:, :] = var[:, idx]
    copy_attrs(var, out)

# Global attributes: preserve originals, then stamp provenance.
flux_vars = [name for name in src.variables if name != "xtime"]
for a in src.ncattrs():
    dst.setncattr(a, src.getncattr(a))
dst.setncattr("subset_source", SRC.split("/")[-1])
dst.setncattr(
    "subset_note",
    "Reader-test fixture for MIEM. Heavily reduced from the full "
    "MPAS x1.163842 grid by keeping every {}th cell ({} of {} cells); "
    "all {} anthropogenic {} sectors ({}) and all 12 months retained. "
    "Values are unmodified real CAMS-GLOB-ANT emissions.".format(
        STRIDE, len(idx), n_cells_full, len(flux_vars), SPECIES, ", ".join(flux_vars)
    ),
)
dst.close()
src.close()
print("wrote {} cells x {} times".format(len(idx), 12))
