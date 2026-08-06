// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "internal/uptempo_reader.hpp"

#include "internal/netcdf_io.hpp"
#include "internal/time_utils.hpp"

#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <netcdf.h>
#include <string>
#include <utility>
#include <vector>

namespace miem
{

  namespace
  {

    // Strip the fixed-width padding NetCDF char variables carry: cut at the
    // first NUL, then drop trailing spaces.
    std::string TrimFixedWidth(const std::string& raw)
    {
      std::string s = raw.substr(0, raw.find('\0'));
      while (!s.empty() && s.back() == ' ')
      {
        s.pop_back();
      }
      return s;
    }

    // Parse an MPAS `xtime` stamp -- "YYYY-MM-DD_HH:MM:SS", with the time part
    // optional -- into seconds since the Unix epoch (UTC).  Returns false for
    // anything it cannot fully account for, so a malformed stamp is reported
    // rather than silently mis-dated.  (std::chrono::parse would be tidier but
    // isn't available across all of our toolchains yet, matching the ECCAD
    // reader's hand-rolled CF parser.)
    bool ParseMpasXtime(const std::string& stamp, double& seconds_out)
    {
      const char* cursor = stamp.data();
      const char* const end = stamp.data() + stamp.size();

      const auto take = [&](int& out, char delim) -> bool
      {
        const auto [next, ec] = std::from_chars(cursor, end, out);
        if (ec != std::errc{})
        {
          return false;
        }
        cursor = next;
        if (delim != '\0')
        {
          if (cursor == end || *cursor != delim)
          {
            return false;
          }
          ++cursor;
        }
        return true;
      };

      int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
      if (!take(year, '-') || !take(month, '-') || !take(day, '\0'))
      {
        return false;
      }

      // MPAS separates the date and time with '_'; tolerate ' ' / 'T' too.
      if (cursor != end && (*cursor == '_' || *cursor == ' ' || *cursor == 'T'))
      {
        ++cursor;
        if (!take(hour, ':') || !take(minute, '\0'))
        {
          return false;
        }
        if (cursor != end && *cursor == ':')
        {
          ++cursor;
          if (!take(second, '\0'))
          {
            return false;
          }
        }
      }

      // Reject anything left over rather than silently ignoring a bad stamp.
      if (cursor != end)
      {
        return false;
      }

      const std::chrono::sys_seconds tp = UtcTimePoint(year, month, day, hour, minute, second);
      seconds_out = static_cast<double>(tp.time_since_epoch().count());
      return true;
    }

  }  // namespace

  UptempoReader::UptempoReader(UptempoReader&& other) noexcept
      : ncid_(other.ncid_),
        file_path_(std::move(other.file_path_)),
        n_time_steps_(other.n_time_steps_),
        n_cells_(other.n_cells_),
        time_dim_id_(other.time_dim_id_),
        cell_dim_id_(other.cell_dim_id_),
        available_species_(std::move(other.available_species_))
  {
    other.ncid_ = -1;
  }

  UptempoReader& UptempoReader::operator=(UptempoReader&& other) noexcept
  {
    if (this != &other)
    {
      Close();
      ncid_ = other.ncid_;
      file_path_ = std::move(other.file_path_);
      n_time_steps_ = other.n_time_steps_;
      n_cells_ = other.n_cells_;
      time_dim_id_ = other.time_dim_id_;
      cell_dim_id_ = other.cell_dim_id_;
      available_species_ = std::move(other.available_species_);
      other.ncid_ = -1;
    }
    return *this;
  }

  void UptempoReader::Open(const std::string& file_path)
  {
    if (ncid_ >= 0)
    {
      Close();
    }

    file_path_ = file_path;

    const int status = nc_open(file_path.c_str(), NC_NOWRITE, &ncid_);
    if (status != NC_NOERR)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_IO,
          MIEM_IO_ERROR_CODE_FILE_NOT_FOUND,
          "Failed to open NetCDF file: " + file_path + " — " + nc_strerror(status));
    }

    DetectDimensions();
    DiscoverSpecies();
  }

  void UptempoReader::Close()
  {
    if (ncid_ >= 0)
    {
      nc_close(ncid_);
      ncid_ = -1;
    }
    available_species_.clear();
    n_time_steps_ = 0;
    n_cells_ = 0;
    time_dim_id_ = -1;
    cell_dim_id_ = -1;
  }

  void UptempoReader::DetectDimensions()
  {
    // The MPAS cell dimension is required; without it the file is not on a
    // mesh MIEM can read.
    if (nc_inq_dimid(ncid_, "nCells", &cell_dim_id_) != NC_NOERR)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_IO,
          MIEM_IO_ERROR_CODE_INVALID_FORMAT,
          "UptempoReader: dimension 'nCells' missing in: " + file_path_);
    }
    std::size_t cell_len;
    MIEM_NC_CHECK(nc_inq_dimlen(ncid_, cell_dim_id_, &cell_len));
    n_cells_ = static_cast<int>(cell_len);

    // The time dimension is optional -- a single-snapshot file may omit it.
    if (nc_inq_dimid(ncid_, "Time", &time_dim_id_) == NC_NOERR)
    {
      std::size_t time_len;
      MIEM_NC_CHECK(nc_inq_dimlen(ncid_, time_dim_id_, &time_len));
      n_time_steps_ = static_cast<int>(time_len);
    }
    else
    {
      time_dim_id_ = -1;
      n_time_steps_ = 1;
    }
  }

  void UptempoReader::DiscoverSpecies()
  {
    available_species_.clear();

    int n_vars;
    MIEM_NC_CHECK(nc_inq_nvars(ncid_, &n_vars));

    // A flux field is any floating-point variable laid out on the mesh:
    // (Time, nCells), or (nCells) for a single-snapshot file. The variable's
    // own name is the inventory species name -- the reader makes no
    // assumption about the species or sector naming scheme, so it works for
    // any UPTEMPO-remapped inventory, not just one product.
    for (int varid = 0; varid < n_vars; ++varid)
    {
      nc_type var_type;
      MIEM_NC_CHECK(nc_inq_vartype(ncid_, varid, &var_type));
      if (var_type != NC_FLOAT && var_type != NC_DOUBLE)
      {
        continue;  // skip xtime (char) and any integer index variables
      }

      int ndims;
      MIEM_NC_CHECK(nc_inq_varndims(ncid_, varid, &ndims));
      int dimids[NC_MAX_VAR_DIMS];
      MIEM_NC_CHECK(nc_inq_vardimid(ncid_, varid, dimids));

      const bool on_mesh_with_time =
          time_dim_id_ >= 0 && ndims == 2 && dimids[0] == time_dim_id_ && dimids[1] == cell_dim_id_;
      const bool on_mesh_snapshot = ndims == 1 && dimids[0] == cell_dim_id_;
      if (!on_mesh_with_time && !on_mesh_snapshot)
      {
        continue;
      }

      char raw_name[NC_MAX_NAME + 1];
      MIEM_NC_CHECK(nc_inq_varname(ncid_, varid, raw_name));
      available_species_.emplace_back(raw_name);
    }
  }

  std::vector<std::string> UptempoReader::QuerySpecies() const
  {
    return available_species_;
  }

  std::vector<double> UptempoReader::GetTimeValues() const
  {
    std::vector<double> times(n_time_steps_, 0.0);

    int xtime_varid;
    const int status = nc_inq_varid(ncid_, "xtime", &xtime_varid);
    if (status != NC_NOERR)
    {
      // A single-snapshot file may legitimately omit xtime -- treat as t = 0.
      // A multi-step file without it is malformed: refuse to fabricate times.
      if (n_time_steps_ > 1)
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_IO,
            MIEM_IO_ERROR_CODE_INVALID_FORMAT,
            "UptempoReader: file '" + file_path_ +
                "' has a 'Time' dimension "
                "of length " +
                std::to_string(n_time_steps_) +
                " but no 'xtime' variable; refusing to fabricate time "
                "coordinates.");
      }
      return times;  // legitimate single-snapshot file
    }

    const std::string calendar_str = ReadTextAttribute(ncid_, xtime_varid, "calendar");
    if (!IsAcceptedCalendar(calendar_str))
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_IO,
          MIEM_IO_ERROR_CODE_UNSUPPORTED_CALENDAR,
          "UptempoReader: unsupported time calendar '" + calendar_str + "' in " + file_path_ +
              ". v1 supports only gregorian / proleptic_gregorian / standard / "
              "missing.");
    }

    // xtime is char(Time, StrLen); read the whole variable, then slice each
    // fixed-width row out and parse it.
    int xtime_ndims;
    MIEM_NC_CHECK(nc_inq_varndims(ncid_, xtime_varid, &xtime_ndims));
    int xtime_dims[NC_MAX_VAR_DIMS];
    MIEM_NC_CHECK(nc_inq_vardimid(ncid_, xtime_varid, xtime_dims));

    std::size_t total_chars = 1;
    for (int i = 0; i < xtime_ndims; ++i)
    {
      std::size_t dim_len;
      MIEM_NC_CHECK(nc_inq_dimlen(ncid_, xtime_dims[i], &dim_len));
      total_chars *= dim_len;
    }
    std::size_t str_len;
    MIEM_NC_CHECK(nc_inq_dimlen(ncid_, xtime_dims[xtime_ndims - 1], &str_len));

    std::string buffer(total_chars, '\0');
    MIEM_NC_CHECK(nc_get_var_text(ncid_, xtime_varid, buffer.data()));

    for (int t = 0; t < n_time_steps_; ++t)
    {
      const std::string stamp = TrimFixedWidth(buffer.substr(static_cast<std::size_t>(t) * str_len, str_len));
      double seconds = 0.0;
      if (!ParseMpasXtime(stamp, seconds))
      {
        throw MiemException(
            MIEM_ERROR_CATEGORY_IO,
            MIEM_IO_ERROR_CODE_INVALID_TIME_UNITS,
            "UptempoReader: could not parse xtime stamp '" + stamp + "' in " + file_path_ +
                " (expected YYYY-MM-DD_HH:MM:SS).");
      }
      times[t] = seconds;
    }

    return times;
  }

  void UptempoReader::ReadFlux(
      int time_index,
      const std::vector<std::string>& species_names,
      std::vector<Real>& flux_out,
      int& n_cells_out) const
  {
    ReadFluxSelected(time_index, species_names, {}, flux_out, n_cells_out);
  }

  void UptempoReader::ReadFluxSelected(
      int time_index,
      const std::vector<std::string>& species_names,
      const std::vector<int>& selected_global_cell_ids,
      std::vector<Real>& flux_out,
      int& n_cells_out) const
  {
    if (ncid_ < 0)
    {
      throw MiemException(MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_FILE_NOT_FOUND, "UptempoReader: file not open");
    }

    n_cells_out = selected_global_cell_ids.empty()
                      ? n_cells_
                      : static_cast<int>(selected_global_cell_ids.size());
    const int n_species = static_cast<int>(species_names.size());
    flux_out.assign(static_cast<std::size_t>(n_species) * n_cells_out, Real{ 0 });

    for (int isp = 0; isp < n_species; ++isp)
    {
      // UPTEMPO variables are named by the inventory directly, with no prefix.
      int varid;
      const int look = nc_inq_varid(ncid_, species_names[isp].c_str(), &varid);
      if (look != NC_NOERR)
      {
        continue;  // species absent in this file — leave zeros
      }

      int ndims;
      MIEM_NC_CHECK(nc_inq_varndims(ncid_, varid, &ndims));

      std::vector<Real> raw;
      NcGetSelectedCells(
          ncid_, varid, ndims, time_index, n_cells_, selected_global_cell_ids, raw);

      Real fill_value = Real{ 0 };
      bool has_fill = false;
      Real fv{};
      if (NcGetAttFill(ncid_, varid, "_FillValue", &fv) == NC_NOERR)
      {
        fill_value = fv;
        has_fill = true;
      }

      for (int ic = 0; ic < n_cells_out; ++ic)
      {
        Real val = raw[ic];
        // UPTEMPO files mark empty (e.g. ocean) cells with a NaN _FillValue.
        // NaN never compares equal to itself, so test it explicitly before
        // the value comparison.
        if (std::isnan(static_cast<double>(val)) || (has_fill && val == fill_value))
        {
          val = Real{ 0 };
        }
        flux_out[static_cast<std::size_t>(isp) * n_cells_out + ic] = val;
      }
    }
  }

  InventoryGridMetadata UptempoReader::ReadGridMetadata(
      const std::vector<int>& selected_global_cell_ids,
      bool require_exact_grid) const
  {
    if (ncid_ < 0)
    {
      throw MiemException(MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_FILE_NOT_FOUND, "UptempoReader: file not open");
    }
    return ReadInventoryGridMetadata(ncid_, n_cells_, selected_global_cell_ids, require_exact_grid);
  }

}  // namespace miem
