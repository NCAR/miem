#pragma once

#include <string>
#include <vector>

#include "miem/dataset_descriptor.hpp"
#include "miem/util/types.hpp"

namespace miem {

// Reads CES (Canonical Emission Standard) compliant NetCDF files,
// or non-standard files adapted through a DatasetDescriptor.
class CESReader {
 public:
  CESReader() = default;
  ~CESReader() { Close(); }

  // Non-copyable (owns a NetCDF file handle)
  CESReader(const CESReader&) = delete;
  CESReader& operator=(const CESReader&) = delete;
  CESReader(CESReader&& other) noexcept;
  CESReader& operator=(CESReader&& other) noexcept;

  // Open a NetCDF file. If descriptor is provided, use it for non-CES files.
  // If the file is CES-compliant, descriptor overrides are ignored.
  void Open(const std::string& file_path,
            const DatasetDescriptor& descriptor = DatasetDescriptor::Default());

  void Close();

  // Discover available species in the file
  std::vector<std::string> QuerySpecies() const;

  // Get the number of time steps in the file
  int NumTimeSteps() const { return n_time_steps_; }

  // Get time values (in seconds since reference, or as stored)
  std::vector<double> GetTimeValues() const;

  // Read flux data for all species at a given time index.
  // Returns data as (n_species * n_cells) array in kg/m^2/s.
  void ReadFlux(int time_index,
                const std::vector<std::string>& species_names,
                std::vector<Real>& flux_out,
                int& n_cells_out) const;

  bool IsOpen() const { return ncid_ >= 0; }

  int NumCells() const { return n_cells_; }

 private:
  int ncid_ = -1;
  std::string file_path_;
  DatasetDescriptor descriptor_;
  bool is_ces_compliant_ = false;
  int n_time_steps_ = 0;
  int n_cells_ = 0;
  std::vector<std::string> available_species_;

  void DetectFormat();
  void DiscoverSpecies();
  std::string CanonicalName(const std::string& var_name) const;
};

}  // namespace miem
