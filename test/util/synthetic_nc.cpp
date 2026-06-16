// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "synthetic_nc.hpp"

#include <netcdf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace miem_test {

namespace {

void NcCheck(int status, const std::string& where)
{
  if (status != NC_NOERR)
  {
    throw std::runtime_error(where + ": " + nc_strerror(status));
  }
}

}  // namespace

TempDir::TempDir()
{
  namespace fs = std::filesystem;
  // Portable unique temp directory (no POSIX mkdtemp / hardcoded /tmp).
  std::random_device rd;
  for (int attempt = 0; attempt < 100; ++attempt)
  {
    const fs::path candidate =
        fs::temp_directory_path() / ("miem_test_" + std::to_string(rd()));
    std::error_code ec;
    if (fs::create_directory(candidate, ec))
    {
      path_ = candidate.string();
      return;
    }
  }
  throw std::runtime_error("TempDir: could not create a unique temp directory");
}

TempDir::~TempDir()
{
  if (!path_.empty())
  {
    // Best-effort cleanup; ignore errors so destructors never throw.
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
}

std::string TempDir::File(const std::string& name) const
{
  return path_ + "/" + name;
}

void CreateTestNetCDF(const std::string&                       path,
                      int                                      n_times,
                      int                                      n_cells,
                      const std::vector<double>&               time_values,
                      const std::vector<std::string>&          species,
                      const std::vector<std::vector<double>>&  flux_data,
                      const SyntheticNcOptions&                opts)
{
  int ncid = -1;
  NcCheck(nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid), "nc_create");

  // Global attributes.  Either the modern eccad_version or the legacy
  // ses_version (or neither, by setting both empty in opts) — the reader
  // accepts both.
  if (!opts.eccad_version.empty())
  {
    nc_put_att_text(ncid, NC_GLOBAL, "eccad_version",
                    opts.eccad_version.size(), opts.eccad_version.c_str());
  }
  if (!opts.ses_version.empty())
  {
    nc_put_att_text(ncid, NC_GLOBAL, "ses_version",
                    opts.ses_version.size(), opts.ses_version.c_str());
  }
  const char* conv = "CF-1.8";
  nc_put_att_text(ncid, NC_GLOBAL, "Conventions", std::strlen(conv), conv);

  // Dimensions
  int time_dim = -1;
  int cell_dim = -1;
  NcCheck(nc_def_dim(ncid, "time",
                     static_cast<std::size_t>(n_times), &time_dim),
          "nc_def_dim(time)");
  NcCheck(nc_def_dim(ncid, "n_cells",
                     static_cast<std::size_t>(n_cells), &cell_dim),
          "nc_def_dim(n_cells)");

  // Time variable (omitted when opts.omit_time_variable is set)
  int time_varid = -1;
  if (!opts.omit_time_variable)
  {
    NcCheck(nc_def_var(ncid, "time", NC_DOUBLE, 1, &time_dim, &time_varid),
            "nc_def_var(time)");
    if (!opts.time_units.empty())
    {
      nc_put_att_text(ncid, time_varid, "units",
                      opts.time_units.size(), opts.time_units.c_str());
    }
    if (!opts.calendar.empty())
    {
      nc_put_att_text(ncid, time_varid, "calendar",
                      opts.calendar.size(), opts.calendar.c_str());
    }
  }

  // Species flux variables: (time, n_cells)
  std::vector<int> flux_varids(species.size(), -1);
  int dims2d[2] = { time_dim, cell_dim };
  for (std::size_t i = 0; i < species.size(); ++i)
  {
    const std::string var_name = "emi_" + species[i];
    NcCheck(nc_def_var(ncid, var_name.c_str(), NC_DOUBLE, 2, dims2d,
                       &flux_varids[i]),
            "nc_def_var(" + var_name + ")");
  }

  NcCheck(nc_enddef(ncid), "nc_enddef");

  // Write time values (only if the variable exists).
  if (time_varid >= 0)
  {
    NcCheck(nc_put_var_double(ncid, time_varid, time_values.data()),
            "nc_put_var_double(time)");
  }

  // Write flux data per species
  for (std::size_t i = 0; i < species.size(); ++i)
  {
    NcCheck(nc_put_var_double(ncid, flux_varids[i], flux_data[i].data()),
            "nc_put_var_double(emi_" + species[i] + ")");
  }

  NcCheck(nc_close(ncid), "nc_close");
}

void CreateUptempoTestNetCDF(const std::string&                      path,
                             int                                     n_times,
                             int                                     n_cells,
                             const std::vector<std::string>&         xtime_stamps,
                             const std::vector<std::string>&         variables,
                             const std::vector<std::vector<double>>& flux_data,
                             const UptempoNcOptions&                 opts)
{
  constexpr int kStrLen = 64;

  int ncid = -1;
  NcCheck(nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid), "nc_create");

  // Dimensions: Time, nCells, StrLen.  Deliberately no version attribute --
  // the UPTEMPO layout is identified by its dimensions, not a marker.
  int time_dim = -1;
  int cell_dim = -1;
  int str_dim  = -1;
  NcCheck(nc_def_dim(ncid, "Time",
                     static_cast<std::size_t>(n_times), &time_dim),
          "nc_def_dim(Time)");
  NcCheck(nc_def_dim(ncid, "nCells",
                     static_cast<std::size_t>(n_cells), &cell_dim),
          "nc_def_dim(nCells)");
  NcCheck(nc_def_dim(ncid, "StrLen", kStrLen, &str_dim), "nc_def_dim(StrLen)");

  // MPAS xtime: char(Time, StrLen).
  int xtime_varid = -1;
  if (!opts.omit_xtime)
  {
    int xtime_dims[2] = { time_dim, str_dim };
    NcCheck(nc_def_var(ncid, "xtime", NC_CHAR, 2, xtime_dims, &xtime_varid),
            "nc_def_var(xtime)");
    if (!opts.calendar.empty())
    {
      nc_put_att_text(ncid, xtime_varid, "calendar",
                      opts.calendar.size(), opts.calendar.c_str());
    }
  }

  // Flux variables: (Time, nCells), named exactly as requested.
  std::vector<int> flux_varids(variables.size(), -1);
  int dims2d[2] = { time_dim, cell_dim };
  const double nan_value = std::nan("");
  for (std::size_t i = 0; i < variables.size(); ++i)
  {
    NcCheck(nc_def_var(ncid, variables[i].c_str(), NC_DOUBLE, 2, dims2d,
                       &flux_varids[i]),
            "nc_def_var(" + variables[i] + ")");
    if (opts.nan_fill)
    {
      nc_put_att_double(ncid, flux_varids[i], "_FillValue", NC_DOUBLE, 1,
                        &nan_value);
    }
  }

  NcCheck(nc_enddef(ncid), "nc_enddef");

  // Write xtime, padding each stamp to the fixed StrLen width.
  if (xtime_varid >= 0)
  {
    std::string buffer(static_cast<std::size_t>(n_times) * kStrLen, '\0');
    for (int t = 0; t < n_times && t < static_cast<int>(xtime_stamps.size());
         ++t)
    {
      const std::string& stamp = xtime_stamps[t];
      const std::size_t   len =
          std::min<std::size_t>(stamp.size(), kStrLen);
      std::memcpy(buffer.data() + static_cast<std::size_t>(t) * kStrLen,
                  stamp.data(), len);
    }
    NcCheck(nc_put_var_text(ncid, xtime_varid, buffer.data()),
            "nc_put_var_text(xtime)");
  }

  // Write flux data per variable.
  for (std::size_t i = 0; i < variables.size(); ++i)
  {
    NcCheck(nc_put_var_double(ncid, flux_varids[i], flux_data[i].data()),
            "nc_put_var_double(" + variables[i] + ")");
  }

  NcCheck(nc_close(ncid), "nc_close");
}

}  // namespace miem_test
