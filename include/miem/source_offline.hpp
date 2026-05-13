// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Offline emission source: reads pre-regridded ECCAD NetCDF data, applies
// the species map, and temporally interpolates between bracketing slices.
//
// Out-of-range times are a hard error (`TimeOutOfRange`) — the
// climatological wrap-around behavior present in `feature/scaffolding`
// has been removed (plan §D5, user decision 1).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miem/config.hpp"
#include "miem/source.hpp"
#include "miem/species_map.hpp"
#include "miem/temporal_interpolator.hpp"

namespace miem {

// Forward declaration — `ECCADReader` lives in src/internal/ so its
// throwing IO surface stays out of the installed public header set.
class ECCADReader;

class OfflineEmissionSource : public EmissionSource
{
 public:
  OfflineEmissionSource();
  explicit OfflineEmissionSource(const SourceConfig& config);
  ~OfflineEmissionSource() override;

  OfflineEmissionSource(OfflineEmissionSource&&) noexcept;
  OfflineEmissionSource& operator=(OfflineEmissionSource&&) noexcept;

  std::vector<std::string> QuerySpecies() const override;
  Result<void> Update(double                    time_current,
                      int                       n_cells,
                      std::vector<Real>&        flux_out,
                      std::vector<std::string>& species_names_out) override;
  const std::string& Name() const override { return name_; }

 private:
  std::string                  name_;
  SourceConfig                 config_;
  // Held by unique_ptr so the public header only needs a forward
  // declaration of the internal ECCADReader type.
  std::unique_ptr<ECCADReader> reader_;
  SpeciesMap                   species_map_;
  TemporalInterpolator         interpolator_;

  std::vector<std::string> inventory_species_;
  std::vector<std::string> mechanism_species_;

  bool brackets_loaded_ = false;

  Result<void> LoadBrackets(double time_current, int n_cells);
  std::string  ResolveFilePath(double time) const;
};

}  // namespace miem
