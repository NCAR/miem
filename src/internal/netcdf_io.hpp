// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// NetCDF helpers shared by MIEM's inventory readers: the error-checking
// macro, a text-attribute reader, the accepted-calendar policy, and
// `Real`-typed get overloads so the element type selects the matching
// NetCDF call through overload resolution rather than preprocessor
// branching.
//
// Private to src/internal/: this header includes <netcdf.h>, so it never
// reaches MIEM's public install surface.
#pragma once

#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <cstddef>
#include <netcdf.h>
#include <string>

// Throw MiemException (IO) on a non-NC_NOERR NetCDF status.
#define MIEM_NC_CHECK(call)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    const int nc_status_ = (call);                                                                                     \
    if (nc_status_ != NC_NOERR)                                                                                        \
    {                                                                                                                  \
      throw ::miem::MiemException(                                                                                     \
          MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_NETCDF, std::string("NetCDF error: ") + nc_strerror(nc_status_)); \
    }                                                                                                                  \
  } while (0)

namespace miem
{

  // Accept the gregorian calendar family (or a missing attribute). Anything
  // else is rejected by the caller with UnsupportedCalendar to keep v1 free
  // of multi-calendar arithmetic.
  inline bool IsAcceptedCalendar(const std::string& cal)
  {
    return cal.empty() || cal == "gregorian" || cal == "proleptic_gregorian" || cal == "standard";
  }

  // Read a text attribute; returns empty if it is absent, empty, or
  // implausibly large (a guard against pathological files).
  inline std::string ReadTextAttribute(int ncid, int varid, const std::string& name)
  {
    std::size_t att_len = 0;
    const int status = nc_inq_attlen(ncid, varid, name.c_str(), &att_len);
    if (status != NC_NOERR || att_len == 0 || att_len > 4096)
    {
      return {};
    }
    std::string value(att_len, '\0');
    if (nc_get_att_text(ncid, varid, name.c_str(), value.data()) != NC_NOERR)
    {
      return {};
    }
    return value;
  }

  // Type-dispatched NetCDF readers so the `Real` element type selects the
  // matching call via overload resolution, without preprocessor branching.
  inline int NcGetVara(int ncid, int varid, const std::size_t* start, const std::size_t* count, double* out)
  {
    return nc_get_vara_double(ncid, varid, start, count, out);
  }
  inline int NcGetVara(int ncid, int varid, const std::size_t* start, const std::size_t* count, float* out)
  {
    return nc_get_vara_float(ncid, varid, start, count, out);
  }
  inline int NcGetVar(int ncid, int varid, double* out)
  {
    return nc_get_var_double(ncid, varid, out);
  }
  inline int NcGetVar(int ncid, int varid, float* out)
  {
    return nc_get_var_float(ncid, varid, out);
  }
  inline int NcGetAttFill(int ncid, int varid, const char* name, double* out)
  {
    return nc_get_att_double(ncid, varid, name, out);
  }
  inline int NcGetAttFill(int ncid, int varid, const char* name, float* out)
  {
    return nc_get_att_float(ncid, varid, name, out);
  }

}  // namespace miem
