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
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <netcdf.h>
#include <numeric>
#include <set>
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
  template<class ValueType>
  inline void NcGetSelectedCells(
      int ncid,
      int varid,
      int ndims,
      int time_index,
      int global_n_cells,
      const std::vector<int>& selected_global_cell_ids,
      std::vector<ValueType>& out)
  {
    if (selected_global_cell_ids.empty())
    {
      out.assign(static_cast<std::size_t>(global_n_cells), ValueType{ 0 });
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

    out.assign(selected_global_cell_ids.size(), ValueType{ 0 });
    std::size_t run_begin = 0;
    while (run_begin < ordered.size())
    {
      std::size_t run_end = run_begin + 1;
      while (run_end < ordered.size() && ordered[run_end].first == ordered[run_end - 1].first + 1)
        ++run_end;

      const std::size_t run_size = run_end - run_begin;
      std::vector<ValueType> run_values(run_size, ValueType{ 0 });
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

  inline bool NcHasAttribute(int ncid, int varid, const char* name)
  {
    return nc_inq_att(ncid, varid, name, nullptr, nullptr) == NC_NOERR;
  }

  inline bool NcHasVariable(int ncid, const char* name)
  {
    int varid = -1;
    return nc_inq_varid(ncid, name, &varid) == NC_NOERR;
  }

  inline std::string TrimMetadataText(std::string value)
  {
    while (!value.empty() && (value.back() == '\0' || std::isspace(static_cast<unsigned char>(value.back()))))
    {
      value.pop_back();
    }
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
      ++begin;
    }
    return value.substr(begin);
  }

  [[noreturn]] inline void ThrowInvalidGridMetadata(const std::string& message)
  {
    throw MiemException(
        MIEM_ERROR_CATEGORY_VALIDATION,
        MIEM_VALIDATION_ERROR_CODE_INVALID_GRID_METADATA,
        "inventory grid metadata: " + message);
  }

  inline void ValidateCellVariable(int ncid, int varid, int global_n_cells, const std::string& name)
  {
    int ndims = 0;
    MIEM_NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
    if (ndims != 1)
    {
      ThrowInvalidGridMetadata("'" + name + "' must have exactly one cell dimension");
    }

    int dimid = -1;
    MIEM_NC_CHECK(nc_inq_vardimid(ncid, varid, &dimid));
    char dimension_name[NC_MAX_NAME + 1] = {};
    MIEM_NC_CHECK(nc_inq_dimname(ncid, dimid, dimension_name));
    if (std::string(dimension_name) != "nCells" && std::string(dimension_name) != "n_cells")
    {
      ThrowInvalidGridMetadata("'" + name + "' must use the inventory cell dimension");
    }
    std::size_t length = 0;
    MIEM_NC_CHECK(nc_inq_dimlen(ncid, dimid, &length));
    if (length != static_cast<std::size_t>(global_n_cells))
    {
      ThrowInvalidGridMetadata(
          "'" + name + "' has " + std::to_string(length) + " cells; expected " + std::to_string(global_n_cells));
    }
  }

  inline bool IsNumericNetcdfType(nc_type type)
  {
    return type == NC_BYTE || type == NC_UBYTE || type == NC_SHORT || type == NC_USHORT || type == NC_INT ||
           type == NC_UINT || type == NC_INT64 || type == NC_UINT64 || type == NC_FLOAT || type == NC_DOUBLE;
  }

  inline bool IsIntegerNetcdfType(nc_type type)
  {
    return type == NC_BYTE || type == NC_UBYTE || type == NC_SHORT || type == NC_USHORT || type == NC_INT ||
           type == NC_UINT || type == NC_INT64 || type == NC_UINT64;
  }

  inline std::vector<std::int64_t> NcGetSelectedCellIds(
      int ncid,
      int varid,
      int global_n_cells,
      const std::vector<int>& selected_global_cell_ids)
  {
    const std::size_t output_size = selected_global_cell_ids.empty()
                                        ? static_cast<std::size_t>(global_n_cells)
                                        : selected_global_cell_ids.size();
    std::vector<std::int64_t> output(output_size, 0);
    if (selected_global_cell_ids.empty())
    {
      std::vector<long long> raw(output_size, 0);
      MIEM_NC_CHECK(nc_get_var_longlong(ncid, varid, raw.data()));
      std::transform(raw.begin(), raw.end(), output.begin(), [](long long value)
                     { return static_cast<std::int64_t>(value); });
      return output;
    }

    std::vector<std::pair<int, std::size_t>> ordered;
    ordered.reserve(output_size);
    for (std::size_t output_index = 0; output_index < output_size; ++output_index)
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

    std::size_t run_begin = 0;
    while (run_begin < ordered.size())
    {
      std::size_t run_end = run_begin + 1;
      while (run_end < ordered.size() && ordered[run_end].first == ordered[run_end - 1].first + 1)
      {
        ++run_end;
      }
      const std::size_t run_size = run_end - run_begin;
      std::vector<long long> run_values(run_size, 0);
      const std::size_t start[1] = { static_cast<std::size_t>(ordered[run_begin].first) };
      const std::size_t count[1] = { run_size };
      MIEM_NC_CHECK(nc_get_vara_longlong(ncid, varid, start, count, run_values.data()));
      for (std::size_t i = 0; i < run_size; ++i)
      {
        output[ordered[run_begin + i].second] = static_cast<std::int64_t>(run_values[i]);
      }
      run_begin = run_end;
    }
    return output;
  }

  inline InventoryGridField ReadInventoryGridField(
      int ncid,
      const std::string& name,
      int global_n_cells,
      const std::vector<int>& selected_global_cell_ids)
  {
    int varid = -1;
    if (nc_inq_varid(ncid, name.c_str(), &varid) != NC_NOERR)
    {
      ThrowInvalidGridMetadata("missing required cell field '" + name + "'");
    }
    ValidateCellVariable(ncid, varid, global_n_cells, name);
    nc_type variable_type = NC_NAT;
    MIEM_NC_CHECK(nc_inq_vartype(ncid, varid, &variable_type));
    if (!IsNumericNetcdfType(variable_type))
    {
      ThrowInvalidGridMetadata("cell field '" + name + "' must have a numeric type");
    }

    InventoryGridField field;
    field.units_ = TrimMetadataText(ReadTextAttribute(ncid, varid, "units"));
    if (field.units_.empty())
    {
      field.units_ = "<absent>";
    }
    NcGetSelectedCells(
        ncid,
        varid,
        1,
        0,
        global_n_cells,
        selected_global_cell_ids,
        field.values_);
    for (const double value : field.values_)
    {
      if (!std::isfinite(value))
      {
        ThrowInvalidGridMetadata("cell field '" + name + "' contains nonfinite values");
      }
    }
    return field;
  }

  // Read the exact-grid identity written by CheMPAS inventory preparation.
  // Legacy full-grid files return unavailable metadata; selected mode is
  // strict because the host cannot validate a rank-local inventory otherwise.
  inline InventoryGridMetadata ReadInventoryGridMetadata(
      int ncid,
      int global_n_cells,
      const std::vector<int>& selected_global_cell_ids,
      bool require_exact_grid)
  {
    constexpr const char* kAlgorithmAttribute = "chempas_mesh_fingerprint_algorithm";
    constexpr const char* kFingerprintAttribute = "chempas_mesh_sha256";
    constexpr const char* kManifestAttribute = "chempas_mesh_field_manifest";

    const bool has_algorithm = NcHasAttribute(ncid, NC_GLOBAL, kAlgorithmAttribute);
    const bool has_fingerprint = NcHasAttribute(ncid, NC_GLOBAL, kFingerprintAttribute);
    const bool has_manifest = NcHasAttribute(ncid, NC_GLOBAL, kManifestAttribute);
    if (!has_algorithm || !has_fingerprint || !has_manifest)
    {
      if (require_exact_grid)
      {
        ThrowInvalidGridMetadata(
            "selected-cell mode requires chempas_mesh_fingerprint_algorithm, chempas_mesh_sha256, and "
            "chempas_mesh_field_manifest");
      }
      return {};
    }

    InventoryGridMetadata metadata;
    metadata.available_ = true;
    metadata.global_n_cells_ = global_n_cells;
    if (selected_global_cell_ids.empty())
    {
      metadata.selected_global_cell_ids_.resize(static_cast<std::size_t>(global_n_cells));
      std::iota(metadata.selected_global_cell_ids_.begin(), metadata.selected_global_cell_ids_.end(), 1);
    }
    else
    {
      metadata.selected_global_cell_ids_ = selected_global_cell_ids;
    }

    metadata.fingerprint_algorithm_ =
        TrimMetadataText(ReadTextAttribute(ncid, NC_GLOBAL, kAlgorithmAttribute));
    metadata.fingerprint_ = TrimMetadataText(ReadTextAttribute(ncid, NC_GLOBAL, kFingerprintAttribute));
    metadata.field_manifest_ = TrimMetadataText(ReadTextAttribute(ncid, NC_GLOBAL, kManifestAttribute));
    if (metadata.fingerprint_algorithm_ != "chempas-mesh-sha256-v1")
    {
      ThrowInvalidGridMetadata(
          "unsupported fingerprint algorithm '" + metadata.fingerprint_algorithm_ + "'");
    }
    if (metadata.fingerprint_.size() != 64 ||
        !std::all_of(metadata.fingerprint_.begin(), metadata.fingerprint_.end(), [](unsigned char value)
                     { return std::isxdigit(value) != 0; }))
    {
      ThrowInvalidGridMetadata("chempas_mesh_sha256 must contain exactly 64 hexadecimal characters");
    }
    if (metadata.field_manifest_.empty())
    {
      ThrowInvalidGridMetadata("chempas_mesh_field_manifest must not be empty");
    }

    metadata.on_a_sphere_ = TrimMetadataText(ReadTextAttribute(ncid, NC_GLOBAL, "on_a_sphere"));
    std::transform(
        metadata.on_a_sphere_.begin(),
        metadata.on_a_sphere_.end(),
        metadata.on_a_sphere_.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    if (metadata.on_a_sphere_ == "YES")
    {
      metadata.geometry_ = InventoryGridGeometry::Spherical;
    }
    else if (metadata.on_a_sphere_ == "NO")
    {
      metadata.geometry_ = InventoryGridGeometry::Planar;
    }
    else
    {
      ThrowInvalidGridMetadata("global attribute on_a_sphere must normalize to YES or NO");
    }

    metadata.is_periodic_ = TrimMetadataText(ReadTextAttribute(ncid, NC_GLOBAL, "is_periodic"));
    if (metadata.is_periodic_.empty())
    {
      metadata.is_periodic_ = "<absent>";
    }
    else
    {
      std::transform(
          metadata.is_periodic_.begin(),
          metadata.is_periodic_.end(),
          metadata.is_periodic_.begin(),
          [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
      if (metadata.is_periodic_ != "YES" && metadata.is_periodic_ != "NO")
      {
        ThrowInvalidGridMetadata("global attribute is_periodic must normalize to YES or NO");
      }
    }

    if (NcHasAttribute(ncid, NC_GLOBAL, "sphere_radius"))
    {
      metadata.has_sphere_radius_ = true;
      MIEM_NC_CHECK(nc_get_att_double(ncid, NC_GLOBAL, "sphere_radius", &metadata.sphere_radius_));
      if (!std::isfinite(metadata.sphere_radius_))
      {
        ThrowInvalidGridMetadata("sphere_radius must be finite");
      }
    }

    int index_varid = -1;
    if (nc_inq_varid(ncid, "indexToCellID", &index_varid) != NC_NOERR)
    {
      ThrowInvalidGridMetadata("missing required cell field 'indexToCellID'");
    }
    ValidateCellVariable(ncid, index_varid, global_n_cells, "indexToCellID");
    nc_type index_type = NC_NAT;
    MIEM_NC_CHECK(nc_inq_vartype(ncid, index_varid, &index_type));
    if (!IsIntegerNetcdfType(index_type))
    {
      ThrowInvalidGridMetadata("indexToCellID must have an integer type");
    }
    metadata.index_to_cell_id_units_ = TrimMetadataText(ReadTextAttribute(ncid, index_varid, "units"));
    if (metadata.index_to_cell_id_units_.empty())
    {
      metadata.index_to_cell_id_units_ = "<absent>";
    }
    metadata.index_to_cell_id_ =
        NcGetSelectedCellIds(ncid, index_varid, global_n_cells, selected_global_cell_ids);
    std::set<std::int64_t> unique_ids;
    for (const std::int64_t id : metadata.index_to_cell_id_)
    {
      if (id < 1 || id > global_n_cells || !unique_ids.insert(id).second)
      {
        ThrowInvalidGridMetadata("selected indexToCellID values must be unique and within 1..nCells");
      }
    }

    metadata.fields_.emplace(
        "areaCell", ReadInventoryGridField(ncid, "areaCell", global_n_cells, selected_global_cell_ids));
    for (const char* name : { "latCell", "lonCell", "xCell", "yCell", "zCell" })
    {
      if (NcHasVariable(ncid, name))
      {
        metadata.fields_.emplace(
            name, ReadInventoryGridField(ncid, name, global_n_cells, selected_global_cell_ids));
      }
    }

    if (!metadata.IsExactGrid())
    {
      ThrowInvalidGridMetadata("identity is incomplete for its declared geometry class");
    }
    return metadata;
  }

}  // namespace miem
