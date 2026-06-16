// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Interface implemented by MIEM's inventory file readers (the ECCAD
// reader, the UPTEMPO on-mesh reader, and future formats).
// `OfflineEmissionSource` holds one of these and never names a concrete
// reader, so supporting a new on-disk layout is a new reader plus a
// `reader_factory` branch -- not a change to the source pipeline.
//
// Lives in src/internal/ because concrete implementations pull in
// <netcdf.h>, which must stay off MIEM's public install surface.
#pragma once

#include <miem/util/types.hpp>

#include <string>
#include <vector>

namespace miem
{

  class EmissionFileReader
  {
   public:
    virtual ~EmissionFileReader() = default;

    // Open the file.  Throws MiemException (IO) on failure.
    virtual void Open(const std::string& file_path) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    // Number of time steps and grid cells in the open file.
    virtual int NumTimeSteps() const = 0;
    virtual int NumCells() const = 0;

    // Inventory species available in the file: the names a caller passes to
    // ReadFlux and keys its SpeciesMap on.
    virtual std::vector<std::string> QuerySpecies() const = 0;

    // Time coordinate as seconds since the Unix epoch (UTC).  Throws
    // MiemException (IO) on a missing/unsupported time encoding.
    virtual std::vector<double> GetTimeValues() const = 0;

    // Read flux at `time_index` for `species_names` into `flux_out` (resized
    // to species_names.size() * n_cells, row-major [species*n_cells + cell]).
    // Throws MiemException (IO) on NetCDF failure.
    virtual void ReadFlux(
        int time_index,
        const std::vector<std::string>& species_names,
        std::vector<Real>& flux_out,
        int& n_cells_out) const = 0;
  };

}  // namespace miem
