// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "internal/eccad_reader.hpp"
#include "internal/time_utils.hpp"

#include <netcdf.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <ctime>
#include <set>
#include <string>

#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

#define MIEM_NC_CHECK(call)                                                    \
  do {                                                                         \
    const int nc_status_ = (call);                                             \
    if (nc_status_ != NC_NOERR) {                                              \
      throw ::miem::MiemException(MIEM_ERROR_CATEGORY_IO,                      \
                                  MIEM_IO_ERROR_CODE_NETCDF,                   \
                                  std::string("NetCDF error: ") +             \
                                  nc_strerror(nc_status_));                    \
    }                                                                          \
  } while (0)

namespace miem {

namespace {

// Accept any of these calendars (or missing). Anything else is a hard
// UnsupportedCalendar error.
bool IsAcceptedCalendar(const std::string& cal)
{
  return cal.empty() ||
         cal == "gregorian" ||
         cal == "proleptic_gregorian" ||
         cal == "standard";
}

bool ParseCFTimeUnits(const std::string& units_str,
                      double&            multiplier,
                      double&            ref_epoch)
{
  // CF time units are "<unit> since <ISO-8601 datetime>",
  // e.g. "seconds since 2012-01-01 00:00:00".
  const std::string::size_type since = units_str.find(" since ");
  if (since == std::string::npos)
  {
    return false;
  }

  const std::string unit = units_str.substr(0, since);
  if (unit == "seconds" || unit == "second" || unit == "s")
  {
    multiplier = 1.0;
  }
  else if (unit == "minutes" || unit == "minute")
  {
    multiplier = 60.0;
  }
  else if (unit == "hours" || unit == "hour" || unit == "h")
  {
    multiplier = 3600.0;
  }
  else if (unit == "days" || unit == "day" || unit == "d")
  {
    multiplier = 86400.0;
  }
  else
  {
    return false;
  }

  // Parse the reference datetime: "YYYY-MM-DD" with an optional
  // "[ T]HH:MM[:SS]" tail. (std::chrono::parse would be cleaner but is not
  // yet available across our toolchains.)
  const std::string when   = units_str.substr(since + 7);
  const char*       cursor = when.data();
  const char* const end    = when.data() + when.size();

  // Read an integer, then require/consume `delim` (unless delim == '\0').
  const auto take = [&](int& out, char delim) -> bool {
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
  if (cursor != end && (*cursor == ' ' || *cursor == 'T'))
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

  std::tm ref_tm{};
  ref_tm.tm_year = year - 1900;
  ref_tm.tm_mon  = month - 1;
  ref_tm.tm_mday = day;
  ref_tm.tm_hour = hour;
  ref_tm.tm_min  = minute;
  ref_tm.tm_sec  = second;
  ref_epoch      = static_cast<double>(TimeFromUtcTm(ref_tm));
  return true;
}

std::string ReadTextAttribute(int ncid, int varid, const std::string& name)
{
  std::size_t att_len = 0;
  const int   status  = nc_inq_attlen(ncid, varid, name.c_str(), &att_len);
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
int NcGetVara(int ncid, int varid, const std::size_t* start,
              const std::size_t* count, double* out)
{
  return nc_get_vara_double(ncid, varid, start, count, out);
}
int NcGetVara(int ncid, int varid, const std::size_t* start,
              const std::size_t* count, float* out)
{
  return nc_get_vara_float(ncid, varid, start, count, out);
}
int NcGetVar(int ncid, int varid, double* out)
{
  return nc_get_var_double(ncid, varid, out);
}
int NcGetVar(int ncid, int varid, float* out)
{
  return nc_get_var_float(ncid, varid, out);
}
int NcGetAttFill(int ncid, int varid, const char* name, double* out)
{
  return nc_get_att_double(ncid, varid, name, out);
}
int NcGetAttFill(int ncid, int varid, const char* name, float* out)
{
  return nc_get_att_float(ncid, varid, name, out);
}

}  // namespace

ECCADReader::ECCADReader(ECCADReader&& other) noexcept
    : ncid_(other.ncid_),
      file_path_(std::move(other.file_path_)),
      eccad_version_(std::move(other.eccad_version_)),
      n_time_steps_(other.n_time_steps_),
      n_cells_(other.n_cells_),
      available_species_(std::move(other.available_species_))
{
  other.ncid_ = -1;
}

ECCADReader& ECCADReader::operator=(ECCADReader&& other) noexcept
{
  if (this != &other)
  {
    Close();
    ncid_              = other.ncid_;
    file_path_         = std::move(other.file_path_);
    eccad_version_     = std::move(other.eccad_version_);
    n_time_steps_      = other.n_time_steps_;
    n_cells_           = other.n_cells_;
    available_species_ = std::move(other.available_species_);
    other.ncid_        = -1;
  }
  return *this;
}

void ECCADReader::Open(const std::string& file_path)
{
  if (ncid_ >= 0)
  {
    Close();
  }

  file_path_ = file_path;

  const int status = nc_open(file_path.c_str(), NC_NOWRITE, &ncid_);
  if (status != NC_NOERR)
  {
    throw MiemException(MIEM_ERROR_CATEGORY_IO,
                        MIEM_IO_ERROR_CODE_FILE_NOT_FOUND,
                        "Failed to open NetCDF file: " + file_path +
                        " — " + nc_strerror(status));
  }

  DetectFormat();
  DiscoverSpecies();
}

void ECCADReader::Close()
{
  if (ncid_ >= 0)
  {
    nc_close(ncid_);
    ncid_ = -1;
  }
  available_species_.clear();
  n_time_steps_ = 0;
  n_cells_      = 0;
}

void ECCADReader::DetectFormat()
{
  // ECCAD compliance is signalled by the `eccad_version` (preferred) or
  // legacy `ses_version` global attribute. If neither is present the file
  // is not ECCAD/SES-conformant, so refuse rather than guess.
  eccad_version_ = ReadTextAttribute(ncid_, NC_GLOBAL, "eccad_version");
  if (eccad_version_.empty())
  {
    eccad_version_ = ReadTextAttribute(ncid_, NC_GLOBAL, "ses_version");
  }
  if (eccad_version_.empty())
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_INVALID_FORMAT,
        "ECCADReader: file '" + file_path_ +
        "' has neither 'eccad_version' nor 'ses_version' global "
        "attribute; refer to docs/eccad.md.");
  }

  // Time dimension (optional — a single-snapshot file may omit it).
  int        time_dim_id;
  const int  ts = nc_inq_dimid(ncid_, "time", &time_dim_id);
  if (ts == NC_NOERR)
  {
    std::size_t len;
    MIEM_NC_CHECK(nc_inq_dimlen(ncid_, time_dim_id, &len));
    n_time_steps_ = static_cast<int>(len);
  }
  else
  {
    n_time_steps_ = 1;
  }

  // Cell dimension is required.
  int       cell_dim_id;
  const int cs = nc_inq_dimid(ncid_, "n_cells", &cell_dim_id);
  if (cs != NC_NOERR)
  {
    throw MiemException(MIEM_ERROR_CATEGORY_IO,
                        MIEM_IO_ERROR_CODE_INVALID_FORMAT,
                        "ECCADReader: dimension 'n_cells' missing in: " +
                        file_path_);
  }
  std::size_t cell_len;
  MIEM_NC_CHECK(nc_inq_dimlen(ncid_, cell_dim_id, &cell_len));
  n_cells_ = static_cast<int>(cell_len);
}

void ECCADReader::DiscoverSpecies()
{
  available_species_.clear();

  int n_vars;
  MIEM_NC_CHECK(nc_inq_nvars(ncid_, &n_vars));

  // ECCAD names each emission variable "emi_<species>" (e.g. emi_co);
  // strip the prefix to recover the species name.
  static const std::string kPrefix = "emi_";
  std::set<std::string> seen;
  for (int varid = 0; varid < n_vars; ++varid)
  {
    char raw_name[NC_MAX_NAME + 1];
    MIEM_NC_CHECK(nc_inq_varname(ncid_, varid, raw_name));

    const std::string name(raw_name);
    if (name.size() > kPrefix.size() &&
        name.compare(0, kPrefix.size(), kPrefix) == 0)
    {
      std::string species = name.substr(kPrefix.size());
      if (seen.insert(species).second)
      {
        available_species_.push_back(std::move(species));
      }
    }
  }
}

std::vector<std::string> ECCADReader::QuerySpecies() const
{
  return available_species_;
}

std::vector<double> ECCADReader::GetTimeValues() const
{
  std::vector<double> times(n_time_steps_, 0.0);

  int       time_varid;
  const int status = nc_inq_varid(ncid_, "time", &time_varid);
  if (status != NC_NOERR)
  {
    // A single-snapshot file (no `time` dimension, n_time_steps_ == 1)
    // may legitimately omit the `time` variable -- treat as t = 0.
    // Anything else is a malformed file: refuse to silently fabricate
    // zero-valued times.
    if (n_time_steps_ > 1)
    {
      throw MiemException(
          MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_INVALID_FORMAT,
          "ECCADReader: file '" + file_path_ + "' has a 'time' "
          "dimension of length " + std::to_string(n_time_steps_) +
          " but no 'time' variable; refusing to fabricate zero-valued "
          "time coordinates.");
    }
    return times;  // legitimate single-snapshot file
  }

  MIEM_NC_CHECK(nc_get_var_double(ncid_, time_varid, times.data()));

  const std::string units_str    = ReadTextAttribute(ncid_, time_varid, "units");
  const std::string calendar_str = ReadTextAttribute(ncid_, time_varid, "calendar");

  if (units_str.empty())
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_INVALID_TIME_UNITS,
        "ECCADReader: time variable in " + file_path_ +
        " is missing the required 'units' attribute.");
  }

  if (!IsAcceptedCalendar(calendar_str))
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_UNSUPPORTED_CALENDAR,
        "ECCADReader: unsupported time calendar '" + calendar_str +
        "' in " + file_path_ +
        ". v1 supports only gregorian / proleptic_gregorian / standard / "
        "missing.");
  }

  double multiplier = 1.0;
  double ref_epoch  = 0.0;
  if (!ParseCFTimeUnits(units_str, multiplier, ref_epoch))
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_IO, MIEM_IO_ERROR_CODE_INVALID_TIME_UNITS,
        "ECCADReader: invalid CF time units '" + units_str +
        "' in " + file_path_);
  }

  for (auto& t : times)
  {
    t = t * multiplier + ref_epoch;
  }

  return times;
}

void ECCADReader::ReadFlux(int                             time_index,
                           const std::vector<std::string>& species_names,
                           std::vector<Real>&              flux_out,
                           int&                            n_cells_out) const
{
  if (ncid_ < 0)
  {
    throw MiemException(MIEM_ERROR_CATEGORY_IO,
                        MIEM_IO_ERROR_CODE_FILE_NOT_FOUND,
                        "ECCADReader: file not open");
  }

  n_cells_out               = n_cells_;
  const int n_species       = static_cast<int>(species_names.size());
  flux_out.assign(static_cast<std::size_t>(n_species) * n_cells_, Real{ 0 });

  for (int isp = 0; isp < n_species; ++isp)
  {
    const std::string var_name = "emi_" + species_names[isp];

    int       varid;
    const int look = nc_inq_varid(ncid_, var_name.c_str(), &varid);
    if (look != NC_NOERR)
    {
      continue;  // species absent in this file — leave zeros
    }

    int ndims;
    MIEM_NC_CHECK(nc_inq_varndims(ncid_, varid, &ndims));

    std::vector<Real> raw(n_cells_, Real{ 0 });

    if (ndims == 2)
    {
      const std::size_t start[2] = { static_cast<std::size_t>(time_index), 0 };
      const std::size_t count[2] = { 1, static_cast<std::size_t>(n_cells_) };
      MIEM_NC_CHECK(NcGetVara(ncid_, varid, start, count, raw.data()));
    }
    else if (ndims == 1)
    {
      MIEM_NC_CHECK(NcGetVar(ncid_, varid, raw.data()));
    }
    else
    {
      continue;
    }

    Real fill_value = Real{ 0 };
    bool has_fill   = false;
    Real fv{};
    if (NcGetAttFill(ncid_, varid, "_FillValue", &fv) == NC_NOERR)
    {
      fill_value = fv;
      has_fill   = true;
    }

    for (int ic = 0; ic < n_cells_; ++ic)
    {
      Real val = raw[ic];
      if (has_fill && val == fill_value)
      {
        val = Real{ 0 };
      }
      flux_out[static_cast<std::size_t>(isp) * n_cells_ + ic] = val;
    }
  }
}

}  // namespace miem
