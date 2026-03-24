#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miem/util/types.hpp"

namespace miem {

// Abstract base class for emission sources.
// New source types (dust, sea salt, online biogenic) subclass this.
class EmissionSource {
 public:
  virtual ~EmissionSource() = default;

  // Return the list of mechanism species this source provides
  virtual std::vector<std::string> QuerySpecies() const = 0;

  // Update flux data for the current time step.
  // flux_out: (n_species * n_cells) array, accumulated (not overwritten)
  virtual void Update(double time_current, int n_cells,
                      std::vector<Real>& flux_out,
                      std::vector<std::string>& species_names_out) = 0;

  virtual const std::string& Name() const = 0;
};

}  // namespace miem
