// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Abstract base for emission sources.  Subclasses include the v1
// `OfflineEmissionSource`; future online sources (dust, sea salt,
// biogenic) plug in via `SourceFactory::Create`.
#pragma once

#include <string>
#include <vector>

#include "miem/util/result.hpp"
#include "miem/util/types.hpp"

namespace miem {

class EmissionSource
{
 public:
  virtual ~EmissionSource() = default;

  // Mechanism species this source provides.
  virtual std::vector<std::string> QuerySpecies() const = 0;

  // Update flux data for the requested time step.  `flux_out` is resized
  // to `species_names_out.size() * n_cells` and accumulated (not
  // overwritten) so callers can chain sources.
  virtual Result<void> Update(double                    time_current,
                              int                       n_cells,
                              std::vector<Real>&        flux_out,
                              std::vector<std::string>& species_names_out) = 0;

  virtual const std::string& Name() const = 0;
};

}  // namespace miem
