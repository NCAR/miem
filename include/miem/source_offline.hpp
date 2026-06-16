// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Offline emission source: reads pre-regridded ECCAD NetCDF data, applies
// the species map, and temporally interpolates between bracketing slices.
// Times outside the file's range are a hard error (`TimeOutOfRange`);
// there is no climatological wrap-around.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <miem/source_types.hpp>
#include <miem/source.hpp>
#include <miem/species_map.hpp>
#include <miem/temporal_interpolator.hpp>

namespace miem {

// Forward-declared (defined in src/internal/) to keep NetCDF out of this
// public header.  The concrete reader is selected by the source's
// convention at construction time (see reader_factory).
class EmissionFileReader;

class OfflineEmissionSource : public EmissionSource
{
 public:
  OfflineEmissionSource();
  explicit OfflineEmissionSource(const Source& config);
  ~OfflineEmissionSource() override;

  OfflineEmissionSource(OfflineEmissionSource&&) noexcept;
  OfflineEmissionSource& operator=(OfflineEmissionSource&&) noexcept;

  std::vector<std::string> QuerySpecies() const override;
  void Update(double                    time_current,
              int                       n_cells,
              std::vector<Real>&        flux_out,
              std::vector<std::string>& species_names_out) override;
  const std::string& Name() const override { return name_; }

 private:
  std::string                  name_;
  Source                 config_;
  // Held by unique_ptr so the public header only needs a forward
  // declaration of the internal reader interface.
  std::unique_ptr<EmissionFileReader> reader_;
  SpeciesMap                   species_map_;
  TemporalInterpolator         interpolator_;

  std::vector<std::string> inventory_species_;
  std::vector<std::string> mechanism_species_;

  void         LoadBrackets(double time_current, int n_cells);
  std::string  ResolveFilePath(double time) const;
};

}  // namespace miem
