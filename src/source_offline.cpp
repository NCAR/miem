#include "miem/source_offline.hpp"

#include <algorithm>
#include <cmath>

#include "miem/util/error.hpp"

namespace miem {

OfflineEmissionSource::OfflineEmissionSource(const SourceConfig& config)
    : name_(config.name),
      config_(config),
      interpolator_(config.temporal_interpolation) {
  // Load species map
  species_map_ = SpeciesMap(config.species_map_path);
  mechanism_species_ = species_map_.MechanismSpecies();

  // Load descriptor if provided
  if (!config.descriptor_path.empty()) {
    descriptor_ = DatasetDescriptor::FromYAML(config.descriptor_path);
  }
}

void OfflineEmissionSource::Init(const std::string& /*config_path*/) {
  // Already initialized via constructor; this satisfies the interface
}

std::vector<std::string> OfflineEmissionSource::QuerySpecies() const {
  return mechanism_species_;
}

std::string OfflineEmissionSource::ResolveFilePath(double /*time*/) const {
  // For v1, return the file pattern directly.
  // Future: expand {YYYY}{MM}{DD} tokens based on time.
  return config_.file_pattern;
}

void OfflineEmissionSource::LoadBrackets(double time_current) {
  std::string file_path = ResolveFilePath(time_current);

  reader_.Open(file_path, descriptor_);
  inventory_species_ = reader_.QuerySpecies();

  auto times = reader_.GetTimeValues();
  int n_cells = reader_.NumCells();
  int n_times = reader_.NumTimeSteps();

  if (n_times == 0) {
    throw IOError("No time steps in file: " + file_path);
  }

  // Find bracketing time indices
  int left_idx = 0;
  int right_idx = std::min(1, n_times - 1);

  if (n_times > 1) {
    for (int t = 0; t < n_times - 1; ++t) {
      if (times[t] <= time_current && time_current <= times[t + 1]) {
        left_idx = t;
        right_idx = t + 1;
        break;
      }
    }
    // If time is beyond last bracket, use last two
    if (time_current > times[n_times - 1]) {
      left_idx = n_times - 2;
      right_idx = n_times - 1;
    }
  }

  // Read raw inventory data at bracket times
  std::vector<Real> raw_left, raw_right;
  int n_cells_read;
  reader_.ReadFlux(left_idx, inventory_species_, raw_left, n_cells_read);
  reader_.ReadFlux(right_idx, inventory_species_, raw_right, n_cells_read);

  // Apply species mapping: inventory → mechanism
  std::vector<Real> mapped_left, mapped_right;
  species_map_.Apply(raw_left, inventory_species_, mapped_left, n_cells);
  species_map_.Apply(raw_right, inventory_species_, mapped_right, n_cells);

  // Set interpolator brackets
  double time_left = (n_times > 0) ? times[left_idx] : 0.0;
  double time_right = (n_times > 1) ? times[right_idx] : time_left + 1.0;
  interpolator_.SetBracket(time_left, time_right, mapped_left, mapped_right);

  brackets_loaded_ = true;
}

void OfflineEmissionSource::Update(double time_current, int n_cells,
                                   std::vector<Real>& flux_out,
                                   std::vector<std::string>& species_names_out) {
  // Load brackets if needed
  if (!brackets_loaded_ || !interpolator_.CoversTime(time_current)) {
    LoadBrackets(time_current);
  }

  // Interpolate to current time
  std::vector<Real> interpolated;
  interpolator_.Interpolate(time_current, interpolated);

  species_names_out = mechanism_species_;
  int n_mech = static_cast<int>(mechanism_species_.size());

  // Accumulate into flux_out
  flux_out.resize(static_cast<size_t>(n_mech) * n_cells, 0.0);
  for (size_t i = 0; i < interpolated.size() && i < flux_out.size(); ++i) {
    flux_out[i] += interpolated[i];
  }
}

}  // namespace miem
