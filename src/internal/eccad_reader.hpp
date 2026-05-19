// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `ECCADReader` reads ECCAD-conforming NetCDF emission files.  Renamed
// from `SESReader` in this port per user decision (overrides plan's
// "defer rename" guidance).
//
// INTERNAL HEADER.  Lives under `src/internal/` and is NOT installed.
// `ECCADReader` throws `IOError` on open/IO failures, which would
// violate the "no throwing in installed public headers" acceptance
// criterion if the header were exposed.  Downstream consumers only
// see `OfflineEmissionSource`, which forward-declares this type and
// holds it via `std::unique_ptr` so the implementation can stay
// private.
//
// CF-calendar policy: the `time` variable's `calendar` attribute must be
// one of `gregorian` / `proleptic_gregorian` / `standard` / missing.
// Anything else (notably `noleap`, `360_day`, `julian`) is rejected with
// `UnsupportedCalendar` to keep v1 free of multi-calendar arithmetic.
//
// NetCDF I/O exceptions are converted to `IOError` (caught at the
// public boundary).  This header does NOT include `<netcdf.h>` — the
// NetCDF dependency is private to the .cpp.
#pragma once

#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

class ECCADReader
{
 public:
  ECCADReader() = default;
  ~ECCADReader() { Close(); }

  ECCADReader(const ECCADReader&)             = delete;
  ECCADReader& operator=(const ECCADReader&)  = delete;
  ECCADReader(ECCADReader&&) noexcept;
  ECCADReader& operator=(ECCADReader&&) noexcept;

  // Open a NetCDF file.  Throws IOError on failure (caught at the
  // Emissions boundary).
  void Open(const std::string& file_path);

  void Close();

  bool IsOpen() const { return ncid_ >= 0; }

  // Number of time steps in the file.
  int NumTimeSteps() const { return n_time_steps_; }

  // Number of grid cells in the file.
  int NumCells() const { return n_cells_; }

  // Available species names (ECCAD `emi_<species>` variables minus the
  // prefix).
  std::vector<std::string> QuerySpecies() const;

  // Time values, converted to seconds-since-Unix-epoch via the CF
  // `units` attribute.  Throws IOError on missing units / unsupported
  // calendar.
  std::vector<double> GetTimeValues() const;

  // Read flux at `time_index` for the requested species into
  // `flux_out` (resized to species_names.size() * n_cells).  Throws
  // IOError on NetCDF failure.
  void ReadFlux(int                            time_index,
                const std::vector<std::string>& species_names,
                std::vector<Real>&              flux_out,
                int&                            n_cells_out) const;

  // ECCAD format version, populated from the `eccad_version` global
  // attribute (legacy `ses_version` is also accepted for one release
  // but yields a warning in this header's documentation only — the
  // attribute name change is intentional and lands in this port).
  const std::string& EccadVersion() const { return eccad_version_; }

 private:
  int          ncid_       = -1;
  std::string  file_path_;
  std::string  eccad_version_;
  int          n_time_steps_ = 0;
  int          n_cells_      = 0;
  std::vector<std::string> available_species_;

  void DetectFormat();
  void DiscoverSpecies();
};

}  // namespace miem
