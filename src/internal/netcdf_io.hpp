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

#include <algorithm>
#include <cstddef>
#include <netcdf.h>
#include <string>
#include <utility>
#include <vector>

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

  // Read a rank-local cell selection with coalesced contiguous hyperslabs.
  // `selected_global_cell_ids` are one-based and output preserves their
  // caller-provided order. An empty selection reads every global cell.
  inline void NcGetSelectedCells(
      int ncid,
      int varid,
      int ndims,
      int time_index,
      int global_n_cells,
      const std::vector<int>& selected_global_cell_ids,
      std::vector<Real>& out)
  {
    if (selected_global_cell_ids.empty())
    {
      out.assign(static_cast<std::size_t>(global_n_cells), Real{ 0 });
      if (ndims == 2)
      {
        const std::size_t start[2] = { static_cast<std::size_t>(time_index), 0 };
        const std::size_t count[2] = { 1, static_cast<std::size_t>(global_n_cells) };
        MIEM_NC_CHECK(NcGetVara(ncid, varid, start, count, out.data()));
      }
      else if (ndims == 1)
      {
        MIEM_NC_CHECK(NcGetVar(ncid, varid, out.data()));
      }
      else
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_IO,
            MIEM_IO_ERROR_CODE_INVALID_FORMAT,
            "emissions variable must have one or two dimensions");
      }
      return;
    }

    std::vector<std::pair<int, std::size_t>> ordered;
    ordered.reserve(selected_global_cell_ids.size());
    for (std::size_t output_index = 0; output_index < selected_global_cell_ids.size(); ++output_index)
    {
      const int global_id = selected_global_cell_ids[output_index];
      if (global_id < 1 || global_id > global_n_cells)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_VALIDATION,
            MIEM_VALIDATION_ERROR_CODE_INVALID_CELL_SELECTION,
            "selected global cell ID " + std::to_string(global_id) + " is outside inventory bounds");
      }
      ordered.emplace_back(global_id - 1, output_index);
    }
    std::sort(ordered.begin(), ordered.end());
    for (std::size_t i = 1; i < ordered.size(); ++i)
    {
      if (ordered[i - 1].first == ordered[i].first)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_VALIDATION,
            MIEM_VALIDATION_ERROR_CODE_INVALID_CELL_SELECTION,
            "selected global cell IDs must be unique");
      }
    }

    out.assign(selected_global_cell_ids.size(), Real{ 0 });
    std::size_t run_begin = 0;
    while (run_begin < ordered.size())
    {
      std::size_t run_end = run_begin + 1;
      while (run_end < ordered.size() && ordered[run_end].first == ordered[run_end - 1].first + 1)
        ++run_end;

      const std::size_t run_size = run_end - run_begin;
      std::vector<Real> run_values(run_size, Real{ 0 });
      const std::size_t global_start = static_cast<std::size_t>(ordered[run_begin].first);
      if (ndims == 2)
      {
        const std::size_t start[2] = { static_cast<std::size_t>(time_index), global_start };
        const std::size_t count[2] = { 1, run_size };
        MIEM_NC_CHECK(NcGetVara(ncid, varid, start, count, run_values.data()));
      }
      else if (ndims == 1)
      {
        const std::size_t start[1] = { global_start };
        const std::size_t count[1] = { run_size };
        MIEM_NC_CHECK(NcGetVara(ncid, varid, start, count, run_values.data()));
      }
      else
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_IO,
            MIEM_IO_ERROR_CODE_INVALID_FORMAT,
            "emissions variable must have one or two dimensions");
      }

      for (std::size_t i = 0; i < run_size; ++i)
        out[ordered[run_begin + i].second] = run_values[i];
      run_begin = run_end;
    }
  }

}  // namespace miem
