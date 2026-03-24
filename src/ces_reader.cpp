#include "miem/ces_reader.hpp"

#include <netcdf.h>

#include <algorithm>
#include <cstring>

#include "miem/util/error.hpp"

#define NC_CHECK(call)                                                  \
  do {                                                                  \
    int nc_status_ = (call);                                            \
    if (nc_status_ != NC_NOERR) {                                      \
      throw IOError(std::string("NetCDF error: ") + nc_strerror(nc_status_)); \
    }                                                                   \
  } while (0)

namespace miem {

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
  char version_buf[64] = {0};
  int status = nc_get_att_text(ncid_, NC_GLOBAL, "miem_version", version_buf);
  is_ces_compliant_ = (status == NC_NOERR);

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

std::vector<double> CESReader::GetTimeValues() const {
  std::vector<double> times(n_time_steps_, 0.0);

  int time_varid;
  const std::string& time_dim_name =
      is_ces_compliant_ ? "Time" : descriptor_.time_dimension;
  int status = nc_inq_varid(ncid_, time_dim_name.c_str(), &time_varid);
  if (status == NC_NOERR) {
    NC_CHECK(nc_get_var_double(ncid_, time_varid, times.data()));
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

    // Apply unit conversion
    Real conv = descriptor_.unit_conversion_factor;
    for (int ic = 0; ic < n_cells_; ++ic) {
      flux_out[isp * n_cells_ + ic] = raw[ic] * conv;
    }
  }
}

}  // namespace miem
