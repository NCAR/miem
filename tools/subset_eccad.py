#!/usr/bin/env python3
"""Produce a heavily-reduced *real* uptempo-convention emissions fixture.

Takes an MPAS-regridded UPTEMPO-convention emissions file (the flat-mesh
layout MIEM's uptempo/eccad readers consume) and keeps a global stride of
cells -- real values, all variables retained -- shrunk to a couple of MB so
it can live in-repo as a reader test fixture. An optional time stride
further reduces high-cadence (e.g. hourly) files.

Handles any variable shaped (Time, nCells), (Time, StrLen) [xtime],
(nCells,) [a bare cell-index coordinate variable, as FINN files have], or
any other single dimension other than Time/nCells [a bare coordinate
variable unrelated to spatial/temporal reduction, e.g. some CAMS files'
StrLen index array -- copied verbatim]; other shapes raise rather than
silently mis-slicing.

Usage: subset_eccad.py <src.nc> <dst.nc> [stride] [species label] [time_stride]
"""
import sys
import numpy as np
import netCDF4 as nc

SRC = sys.argv[1]
DST = sys.argv[2]
STRIDE = int(sys.argv[3]) if len(sys.argv) > 3 else 40
SPECIES = sys.argv[4] if len(sys.argv) > 4 else "black carbon"
TIME_STRIDE = int(sys.argv[5]) if len(sys.argv) > 5 else 1

src = nc.Dataset(SRC, "r")
n_cells_full = len(src.dimensions["nCells"])
n_time_full = src.dimensions["Time"].size

# Plain Python slices (not numpy fancy-index arrays) so netCDF4 issues a
# single strided hyperslab read (nc_get_vars) per variable instead of
# decomposing a fancy integer-array selection into many small scattered
# reads -- critical for a large source file on slow/networked storage.
cell_sel = slice(0, n_cells_full, STRIDE)
time_sel = slice(0, n_time_full, TIME_STRIDE)
n_cells_out = len(range(0, n_cells_full, STRIDE))
n_time_out = len(range(0, n_time_full, TIME_STRIDE))

dst = nc.Dataset(DST, "w", format="NETCDF4")

# Dimensions
dst.createDimension("Time", None)  # unlimited, matches source
dst.createDimension("nCells", n_cells_out)
dst.createDimension("StrLen", len(src.dimensions["StrLen"]))

def copy_attrs(src_var, dst_var):
    for a in src_var.ncattrs():
        if a == "_FillValue":
            continue  # set at creation time
        dst_var.setncattr(a, src_var.getncattr(a))

for name, var in src.variables.items():
    dims = var.dimensions
    fill = var.getncattr("_FillValue") if "_FillValue" in var.ncattrs() else None
    fill_kwargs = {} if fill is None else dict(fill_value=fill)
    if dims == ("Time", "StrLen"):  # xtime
        out = dst.createVariable(name, var.dtype, dims)
        out[:] = var[time_sel, :]
    elif dims == ("Time", "nCells"):  # <species>(Time, nCells)
        out = dst.createVariable(
            name, var.dtype, dims, zlib=True, complevel=4, **fill_kwargs,
        )
        out[:, :] = var[time_sel, cell_sel]
    elif dims == ("nCells",):  # bare cell-index coordinate var (e.g. FINN's)
        out = dst.createVariable(name, var.dtype, dims, **fill_kwargs)
        out[:] = var[cell_sel]
    elif len(dims) == 1 and dims[0] not in ("Time", "nCells"):
        # Bare coordinate var indexed by some other dimension (e.g. StrLen),
        # unrelated to spatial/temporal reduction -- copy verbatim.
        out = dst.createVariable(name, var.dtype, dims, **fill_kwargs)
        out[:] = var[:]
    else:
        raise ValueError(
            "unhandled variable shape for {!r}: dims={}".format(name, dims)
        )
    copy_attrs(var, out)

# Global attributes: preserve originals, then stamp provenance.
flux_vars = [name for name in src.variables if src.variables[name].dimensions == ("Time", "nCells")]
for a in src.ncattrs():
    dst.setncattr(a, src.getncattr(a))
dst.setncattr("subset_source", SRC.split("/")[-1])
dst.setncattr(
    "subset_note",
    "Reader-test fixture for MIEM. Heavily reduced from the full "
    "MPAS x1.163842 grid by keeping every {}th cell ({} of {} cells) and "
    "every {}th timestep ({} of {} times); all {} {} variables ({}) "
    "retained. Values are unmodified real {} emissions.".format(
        STRIDE, n_cells_out, n_cells_full,
        TIME_STRIDE, n_time_out, n_time_full,
        len(flux_vars), SPECIES, ", ".join(flux_vars), SPECIES,
    ),
)
dst.close()
src.close()
print("wrote {} cells x {} times".format(n_cells_out, n_time_out))
