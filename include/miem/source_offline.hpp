#pragma once

#include <string>
#include <vector>

#include "miem/ces_reader.hpp"
#include "miem/config.hpp"
#include "miem/dataset_descriptor.hpp"
#include "miem/source.hpp"
#include "miem/species_map.hpp"
#include "miem/temporal_interpolator.hpp"

namespace miem {

// Offline emission source: reads pre-regridded NetCDF data, applies species
// mapping and temporal interpolation.
class OfflineEmissionSource : public EmissionSource {
 public:
  OfflineEmissionSource() = default;
  explicit OfflineEmissionSource(const SourceConfig& config);

  std::vector<std::string> QuerySpecies() const override;
  void Update(double time_current, int n_cells,
              std::vector<Real>& flux_out,
              std::vector<std::string>& species_names_out) override;
  const std::string& Name() const override { return name_; }

 private:
  std::string name_;
  SourceConfig config_;
  CESReader reader_;
  SpeciesMap species_map_;
  TemporalInterpolator interpolator_;
  DatasetDescriptor descriptor_;

  std::vector<std::string> inventory_species_;
  std::vector<std::string> mechanism_species_;

  // Cached bracket data (in mechanism species space, post-mapping)
  bool brackets_loaded_ = false;

  void LoadBrackets(double time_current);
  std::string ResolveFilePath(double time) const;
};

}  // namespace miem
