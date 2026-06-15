// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Test-only utility: builds synthetic ECCAD-conforming NetCDF emission
// files and provides an RAII temp directory, so unit tests can assert
// against known inputs and deliberately malformed files that real
// fixtures can't easily provide.
//
// Not installed.  Lives in `test/util/` and is linked into individual
// test executables via the `miem_test_synthetic_nc` interface library.
#pragma once

#include <string>
#include <vector>

namespace miem_test
{

  // RAII unique temporary directory: created on construction (via
  // std::filesystem) and removed on destruction.
  class TempDir
  {
   public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& Path() const
    {
      return path_;
    }
    std::string File(const std::string& name) const;

   private:
    std::string path_;
  };

  // Options bundle for `CreateTestNetCDF`.  Defaults give an ECCAD-conforming
  // file with the modern `eccad_version` attribute, `n_cells` cell dim,
  // `time` time dim + variable, double-precision `emi_<species>` fluxes.
  struct SyntheticNcOptions
  {
    // CF time `units` attribute on the time variable.  Default: seconds
    // since UNIX epoch.  Set to empty to suppress writing the attribute.
    std::string time_units = "seconds since 1970-01-01";

    // CF time `calendar` attribute.  Empty omits the attribute (which is a
    // legal "proleptic_gregorian" treatment per CF and the reader).
    std::string calendar = "gregorian";

    // Global attribute style.  Setting `eccad_version` to non-empty writes
    // it as the modern marker; otherwise `ses_version` is written (legacy
    // marker still accepted by the reader).  Setting both empty is the
    // explicit "neither" case (a file with no version attribute).
    std::string eccad_version = "1.0";
    std::string ses_version;

    // If true, suppress emitting a `time` variable even though the time
    // dimension exists — used to test the missing-time-variable case.
    bool omit_time_variable = false;
  };

  // Create an ECCAD-conforming NetCDF file at `path` containing the
  // requested species data.
  //
  // time_values  : length n_times
  // species      : species names (variable name will be "emi_<name>")
  // flux_data    : species.size() inner vectors, each of length
  //                n_times * n_cells, row-major [t * n_cells + cell]
  //
  // Throws std::runtime_error on NetCDF failure.
  void CreateTestNetCDF(
      const std::string& path,
      int n_times,
      int n_cells,
      const std::vector<double>& time_values,
      const std::vector<std::string>& species,
      const std::vector<std::vector<double>>& flux_data,
      const SyntheticNcOptions& opts = {});

}  // namespace miem_test
