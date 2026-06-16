// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `UptempoReader` reads emission inventories already remapped onto an MPAS
// mesh by the UPTEMPO preprocessor -- the "uptempo" Source convention.
//
// How it differs from the ECCAD convention:
//   - flux fields are whatever (Time, nCells) variables the file carries,
//     keyed by their own variable names (e.g. bc_anth_ene, bc_anth_sum).
//     There is no emi_<species> prefix, and the reader is not tied to any
//     one species or sector naming scheme.
//   - the time axis is the MPAS `xtime` character variable, holding
//     "YYYY-MM-DD_HH:MM:SS" strings, rather than a numeric CF `time`
//     variable with a `units` attribute.
//   - there is no version global attribute to gate on; the dimensions
//     `Time` / `nCells` identify the layout.
//
// CF-calendar policy matches the ECCAD reader: if `xtime` carries a
// `calendar` attribute it must be gregorian / proleptic_gregorian /
// standard, otherwise the file is rejected with
// MIEM_IO_ERROR_CODE_UNSUPPORTED_CALENDAR.
//
// Private to src/internal/: this header pulls in <netcdf.h> transitively,
// so it stays off MIEM's public install surface.
#pragma once

#include <string>
#include <vector>

#include "emission_file_reader.hpp"
#include <miem/util/types.hpp>

namespace miem {

class UptempoReader : public EmissionFileReader
{
 public:
  UptempoReader() = default;
  ~UptempoReader() override { Close(); }

  UptempoReader(const UptempoReader&)            = delete;
  UptempoReader& operator=(const UptempoReader&) = delete;
  UptempoReader(UptempoReader&&) noexcept;
  UptempoReader& operator=(UptempoReader&&) noexcept;

  void Open(const std::string& file_path) override;
  void Close() override;
  bool IsOpen() const override { return ncid_ >= 0; }

  int NumTimeSteps() const override { return n_time_steps_; }
  int NumCells() const override { return n_cells_; }

  std::vector<std::string> QuerySpecies() const override;
  std::vector<double>      GetTimeValues() const override;
  void ReadFlux(int                             time_index,
                const std::vector<std::string>& species_names,
                std::vector<Real>&              flux_out,
                int&                            n_cells_out) const override;

 private:
  int         ncid_         = -1;
  std::string file_path_;
  int         n_time_steps_ = 0;
  int         n_cells_      = 0;
  int         time_dim_id_  = -1;  // -1 when the file is a single snapshot
  int         cell_dim_id_  = -1;
  std::vector<std::string> available_species_;

  void DetectDimensions();
  void DiscoverSpecies();
};

}  // namespace miem
