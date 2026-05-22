// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "synthetic_nc.hpp"

#include <netcdf.h>
#include <unistd.h>     // mkdtemp on POSIX
#include <stdlib.h>     // mkdtemp on some libcs

#include <cstdio>
#include <cstdlib>
#include <cstring>
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
  char tmpl[] = "/tmp/miem_test_XXXXXX";
  const char* result = mkdtemp(tmpl);
  if (!result)
  {
    throw std::runtime_error("mkdtemp failed");
  }
  path_ = result;
}

TempDir::~TempDir()
{
  if (!path_.empty())
  {
    // Best-effort cleanup; ignore return code so destructors never throw.
    const std::string cmd = "rm -rf '" + path_ + "'";
    std::system(cmd.c_str());
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

}  // namespace miem_test
