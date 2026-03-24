#include "miem/ces_reader.hpp"

#include <netcdf.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <regex>
#include <sstream>

#include "miem/util/error.hpp"

#define NC_CHECK(call)                                                  \
  do {                                                                  \
    int nc_status_ = (call);                                            \
    if (nc_status_ != NC_NOERR) {                                      \
      throw IOError(std::string("NetCDF error: ") + nc_strerror(nc_status_)); \
    }                                                                   \
  } while (0)

namespace miem {

CESReader::CESReader(CESReader&& other) noexcept
    : ncid_(other.ncid_),
      file_path_(std::move(other.file_path_)),
      descriptor_(std::move(other.descriptor_)),
      is_ces_compliant_(other.is_ces_compliant_),
      n_time_steps_(other.n_time_steps_),
      n_cells_(other.n_cells_),
      available_species_(std::move(other.available_species_)) {
  other.ncid_ = -1;
}

CESReader& CESReader::operator=(CESReader&& other) noexcept {
  if (this != &other) {
    Close();
    ncid_ = other.ncid_;
    file_path_ = std::move(other.file_path_);
    descriptor_ = std::move(other.descriptor_);
    is_ces_compliant_ = other.is_ces_compliant_;
    n_time_steps_ = other.n_time_steps_;
    n_cells_ = other.n_cells_;
    available_species_ = std::move(other.available_species_);
    other.ncid_ = -1;
  }
  return *this;
}

void CESReader::Open(const std::string& file_path,
                     const DatasetDescriptor& descriptor) {
  if (ncid_ >= 0) {
    Close();
  }

  file_path_ = file_path;
  descriptor_ = descriptor;

  int status = nc_open(file_path.c_str(), NC_NOWRITE, &ncid_);
  if (status != NC_NOERR) {
    throw IOError("Failed to open NetCDF file: " + file_path +
                  " — " + nc_strerror(status));
  }

  DetectFormat();
  DiscoverSpecies();
}

void CESReader::Close() {
  if (ncid_ >= 0) {
    nc_close(ncid_);
    ncid_ = -1;
  }
  available_species_.clear();
  n_time_steps_ = 0;
  n_cells_ = 0;
}

void CESReader::DetectFormat() {
  // Check for CES compliance via global attribute
  size_t att_len = 0;
  int status = nc_inq_attlen(ncid_, NC_GLOBAL, "miem_version", &att_len);
  is_ces_compliant_ = false;
  if (status == NC_NOERR && att_len < 256) {
    std::vector<char> version_buf(att_len + 1, '\0');
    status = nc_get_att_text(ncid_, NC_GLOBAL, "miem_version", version_buf.data());
    is_ces_compliant_ = (status == NC_NOERR);
  }

  // Get time dimension
  int time_dim_id;
  const std::string& time_dim_name =
      is_ces_compliant_ ? "Time" : descriptor_.time_dimension;
  status = nc_inq_dimid(ncid_, time_dim_name.c_str(), &time_dim_id);
  if (status == NC_NOERR) {
    size_t time_len;
    NC_CHECK(nc_inq_dimlen(ncid_, time_dim_id, &time_len));
    n_time_steps_ = static_cast<int>(time_len);
  } else {
    n_time_steps_ = 1;  // No time dimension; treat as single snapshot
  }

  // Get cell dimension
  int cell_dim_id;
  const std::string& cell_dim_name =
      is_ces_compliant_ ? "nCells" : descriptor_.cell_dimension;
  status = nc_inq_dimid(ncid_, cell_dim_name.c_str(), &cell_dim_id);
  if (status != NC_NOERR) {
    throw IOError("Cannot find cell dimension '" + cell_dim_name +
                  "' in file: " + file_path_);
  }
  size_t cell_len;
  NC_CHECK(nc_inq_dimlen(ncid_, cell_dim_id, &cell_len));
  n_cells_ = static_cast<int>(cell_len);
}

void CESReader::DiscoverSpecies() {
  available_species_.clear();

  int n_vars;
  NC_CHECK(nc_inq_nvars(ncid_, &n_vars));

  const std::string& prefix =
      is_ces_compliant_ ? "emi_" : descriptor_.variable_prefix;

  for (int varid = 0; varid < n_vars; ++varid) {
    char var_name[NC_MAX_NAME + 1];
    NC_CHECK(nc_inq_varname(ncid_, varid, var_name));

    std::string name(var_name);
    if (name.substr(0, prefix.size()) == prefix) {
      std::string species = CanonicalName(name);
      available_species_.push_back(species);
    }
  }
}

std::string CESReader::CanonicalName(const std::string& var_name) const {
  // Check descriptor rename map first
  auto it = descriptor_.species_rename.find(var_name);
  if (it != descriptor_.species_rename.end()) {
    // The rename target should be in emi_X format; strip prefix
    const std::string& renamed = it->second;
    if (renamed.substr(0, 4) == "emi_") {
      return renamed.substr(4);
    }
    return renamed;
  }

  // Strip the variable prefix to get species name
  const std::string& prefix =
      is_ces_compliant_ ? "emi_" : descriptor_.variable_prefix;
  if (var_name.substr(0, prefix.size()) == prefix) {
    return var_name.substr(prefix.size());
  }
  return var_name;
}

std::vector<std::string> CESReader::QuerySpecies() const {
  return available_species_;
}

namespace {

// Parse CF time units string like "days since 2000-01-01" or
// "seconds since 1970-01-01 00:00:00" into a multiplier (to seconds)
// and a reference epoch (in seconds since Unix epoch).
bool ParseCFTimeUnits(const std::string& units_str,
                      double& multiplier, double& ref_epoch) {
  // Pattern: "<unit> since <date> [<time>]"
  std::regex cf_pattern(
      R"((\w+)\s+since\s+(\d{4})-(\d{1,2})-(\d{1,2})(?:\s+(\d{1,2}):(\d{1,2})(?::(\d{1,2}))?)?)");
  std::smatch match;
  if (!std::regex_search(units_str, match, cf_pattern)) {
    return false;
  }

  std::string unit = match[1].str();
  int year = std::stoi(match[2].str());
  int month = std::stoi(match[3].str());
  int day = std::stoi(match[4].str());
  int hour = match[5].matched ? std::stoi(match[5].str()) : 0;
  int minute = match[6].matched ? std::stoi(match[6].str()) : 0;
  int second = match[7].matched ? std::stoi(match[7].str()) : 0;

  // Unit to seconds multiplier
  if (unit == "seconds" || unit == "second" || unit == "s") {
    multiplier = 1.0;
  } else if (unit == "minutes" || unit == "minute") {
    multiplier = 60.0;
  } else if (unit == "hours" || unit == "hour" || unit == "h") {
    multiplier = 3600.0;
  } else if (unit == "days" || unit == "day" || unit == "d") {
    multiplier = 86400.0;
  } else {
    return false;
  }

  // Convert reference date to seconds since Unix epoch
  std::tm ref_tm{};
  ref_tm.tm_year = year - 1900;
  ref_tm.tm_mon = month - 1;
  ref_tm.tm_mday = day;
  ref_tm.tm_hour = hour;
  ref_tm.tm_min = minute;
  ref_tm.tm_sec = second;
  ref_epoch = static_cast<double>(timegm(&ref_tm));

  return true;
}

}  // anonymous namespace

std::vector<double> CESReader::GetTimeValues() const {
  std::vector<double> times(n_time_steps_, 0.0);

  int time_varid;
  const std::string& time_dim_name =
      is_ces_compliant_ ? "Time" : descriptor_.time_dimension;
  int status = nc_inq_varid(ncid_, time_dim_name.c_str(), &time_varid);
  if (status != NC_NOERR) {
    return times;
  }

  NC_CHECK(nc_get_var_double(ncid_, time_varid, times.data()));

  // Read CF units attribute and convert to seconds since Unix epoch
  size_t units_len = 0;
  status = nc_inq_attlen(ncid_, time_varid, "units", &units_len);
  if (status == NC_NOERR && units_len > 0 && units_len < 1024) {
    std::vector<char> units_buf(units_len + 1, '\0');
    nc_get_att_text(ncid_, time_varid, "units", units_buf.data());
    std::string units_str(units_buf.data(), units_len);

    double multiplier = 1.0;
    double ref_epoch = 0.0;
    if (ParseCFTimeUnits(units_str, multiplier, ref_epoch)) {
      for (auto& t : times) {
        t = t * multiplier + ref_epoch;
      }
    }
  }

  return times;
}

void CESReader::ReadFlux(int time_index,
                         const std::vector<std::string>& species_names,
                         std::vector<Real>& flux_out,
                         int& n_cells_out) const {
  if (ncid_ < 0) {
    throw IOError("CESReader: file not open");
  }

  n_cells_out = n_cells_;
  int n_species = static_cast<int>(species_names.size());
  flux_out.resize(static_cast<size_t>(n_species) * n_cells_, 0.0);

  const std::string& prefix =
      is_ces_compliant_ ? "emi_" : descriptor_.variable_prefix;

  for (int isp = 0; isp < n_species; ++isp) {
    // Build variable name from species name
    std::string var_name = prefix + species_names[isp];

    // Check if descriptor has a reverse rename
    // (we need to map canonical name back to file variable name)
    if (!is_ces_compliant_) {
      for (const auto& [file_var, canonical] : descriptor_.species_rename) {
        std::string canon_species =
            (canonical.substr(0, 4) == "emi_") ? canonical.substr(4) : canonical;
        if (canon_species == species_names[isp]) {
          var_name = file_var;
          break;
        }
      }
    }

    int varid;
    int status = nc_inq_varid(ncid_, var_name.c_str(), &varid);
    if (status != NC_NOERR) {
      continue;  // Species not in file; leave as zero
    }

    // Determine dimensionality
    int ndims;
    NC_CHECK(nc_inq_varndims(ncid_, varid, &ndims));

    std::vector<Real> raw(n_cells_);

    if (ndims == 2) {
      // (Time, nCells)
      size_t start[2] = {static_cast<size_t>(time_index), 0};
      size_t count[2] = {1, static_cast<size_t>(n_cells_)};
#ifdef MIEM_USE_DOUBLE
      NC_CHECK(nc_get_vara_double(ncid_, varid, start, count, raw.data()));
#else
      NC_CHECK(nc_get_vara_float(ncid_, varid, start, count, raw.data()));
#endif
    } else if (ndims == 1) {
      // (nCells) — single time step
#ifdef MIEM_USE_DOUBLE
      NC_CHECK(nc_get_var_double(ncid_, varid, raw.data()));
#else
      NC_CHECK(nc_get_var_float(ncid_, varid, raw.data()));
#endif
    }

    // Check for fill value / missing value and mask to zero
    Real fill_value;
    bool has_fill = false;
#ifdef MIEM_USE_DOUBLE
    double fv;
    if (nc_get_att_double(ncid_, varid, "_FillValue", &fv) == NC_NOERR) {
      fill_value = fv;
      has_fill = true;
    }
#else
    float fv;
    if (nc_get_att_float(ncid_, varid, "_FillValue", &fv) == NC_NOERR) {
      fill_value = fv;
      has_fill = true;
    }
#endif

    // Apply unit conversion and fill value masking
    Real conv = descriptor_.unit_conversion_factor;
    for (int ic = 0; ic < n_cells_; ++ic) {
      Real val = raw[ic];
      if (has_fill && val == fill_value) {
        val = 0.0;
      }
      flux_out[isp * n_cells_ + ic] = val * conv;
    }
  }
}

}  // namespace miem
