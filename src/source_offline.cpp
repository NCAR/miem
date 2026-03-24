#include "miem/source_offline.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

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

std::vector<std::string> OfflineEmissionSource::QuerySpecies() const {
  return mechanism_species_;
}

std::string OfflineEmissionSource::ResolveFilePath(double time) const {
  std::string pattern = config_.file_pattern;

  // If no tokens present, return as-is
  if (pattern.find('{') == std::string::npos) {
    return pattern;
  }

  // Convert time (seconds since epoch) to calendar components
  auto time_t_val = static_cast<std::time_t>(time);
  std::tm tm_val{};
  gmtime_r(&time_t_val, &tm_val);

  // Expand tokens
  auto replace_token = [&](const std::string& token, const std::string& value) {
    std::string::size_type pos;
    while ((pos = pattern.find(token)) != std::string::npos) {
      pattern.replace(pos, token.size(), value);
    }
  };

  std::ostringstream oss;

  oss.str(""); oss << std::setfill('0') << std::setw(4) << (tm_val.tm_year + 1900);
  replace_token("{YYYY}", oss.str());

  oss.str(""); oss << std::setfill('0') << std::setw(2) << (tm_val.tm_mon + 1);
  replace_token("{MM}", oss.str());

  oss.str(""); oss << std::setfill('0') << std::setw(2) << tm_val.tm_mday;
  replace_token("{DD}", oss.str());

  oss.str(""); oss << std::setfill('0') << std::setw(2) << tm_val.tm_hour;
  replace_token("{HH}", oss.str());

  return pattern;
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
    bool found_bracket = false;
    for (int t = 0; t < n_times - 1; ++t) {
      if (times[t] <= time_current && time_current <= times[t + 1]) {
        left_idx = t;
        right_idx = t + 1;
        found_bracket = true;
        break;
      }
    }

    if (!found_bracket) {
      if (time_current > times[n_times - 1]) {
        // Climatological wrap-around: bracket between last and first time
        // steps, treating data as a repeating annual cycle
        left_idx = n_times - 1;
        right_idx = 0;
      } else if (time_current < times[0]) {
        // Before first time step — wrap from last to first
        left_idx = n_times - 1;
        right_idx = 0;
      }
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

  // For climatological wrap-around, adjust right time to be after left
  if (right_idx < left_idx && n_times > 1) {
    // Estimate cycle period from the time span of the file
    double cycle_period = times[n_times - 1] - times[0];
    // Add one step interval to approximate the full cycle
    if (n_times > 1) {
      double avg_step = cycle_period / (n_times - 1);
      time_right = times[n_times - 1] + avg_step;
    }
  }

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

  // Validate cell count consistency
  size_t expected_size = static_cast<size_t>(n_mech) * n_cells;
  if (!interpolated.empty() && interpolated.size() != expected_size) {
    throw ValidationError(
        "OfflineEmissionSource '" + name_ + "': cell count mismatch — "
        "file has " + std::to_string(interpolated.size() / n_mech) +
        " cells but host expects " + std::to_string(n_cells));
  }

  // Accumulate into flux_out
  flux_out.resize(expected_size, 0.0);
  for (size_t i = 0; i < interpolated.size(); ++i) {
    flux_out[i] += interpolated[i];
  }
}

}  // namespace miem
