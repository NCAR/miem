// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/source_offline.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "internal/eccad_reader.hpp"
#include "miem/util/error.hpp"

namespace miem {

namespace {

InterpolationMode AsInterpolationMode(TemporalInterpolation t)
{
  switch (t)
  {
    case TemporalInterpolation::Linear:  return InterpolationMode::kLinear;
    case TemporalInterpolation::Nearest: return InterpolationMode::kNearest;
    case TemporalInterpolation::None:    return InterpolationMode::kNone;
  }
  return InterpolationMode::kLinear;
}

}  // namespace

OfflineEmissionSource::OfflineEmissionSource()
    : reader_(std::make_unique<ECCADReader>())
{
}

OfflineEmissionSource::OfflineEmissionSource(const SourceConfig& config)
    : name_(config.name_),
      config_(config),
      reader_(std::make_unique<ECCADReader>()),
      species_map_(config.species_map_),
      interpolator_(AsInterpolationMode(config.temporal_interpolation_))
{
  mechanism_species_ = species_map_.MechanismSpecies();
}

// Out-of-line special members so the public header only needs a
// forward declaration of ECCADReader.  Defined here (where the type is
// complete) per the standard PIMPL idiom.
OfflineEmissionSource::~OfflineEmissionSource() = default;
OfflineEmissionSource::OfflineEmissionSource(OfflineEmissionSource&&) noexcept
    = default;
OfflineEmissionSource& OfflineEmissionSource::operator=(
    OfflineEmissionSource&&) noexcept = default;

std::vector<std::string> OfflineEmissionSource::QuerySpecies() const
{
  return mechanism_species_;
}

std::string OfflineEmissionSource::ResolveFilePath(double time) const
{
  std::string pattern = config_.file_pattern_;

  if (pattern.find('{') == std::string::npos)
  {
    return pattern;
  }

  auto time_t_val = static_cast<std::time_t>(time);
  std::tm tm_val{};
  gmtime_r(&time_t_val, &tm_val);

  auto replace_token = [&](const std::string& token, const std::string& value) {
    std::string::size_type pos;
    while ((pos = pattern.find(token)) != std::string::npos)
    {
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

Result<void> OfflineEmissionSource::LoadBrackets(double time_current,
                                                 int    n_cells)
{
  const std::string file_path = ResolveFilePath(time_current);

  // ECCADReader throws IOError on open/IO failures; the boundary
  // (EmissionsModule::Run) catches and converts.
  reader_->Open(file_path);
  inventory_species_ = reader_->QuerySpecies();

  const auto times    = reader_->GetTimeValues();
  const int  file_cells = reader_->NumCells();
  const int  n_times  = reader_->NumTimeSteps();

  if (n_times == 0)
  {
    return Result<void>::Error(ErrorCode::FileNotFound,
                               "OfflineEmissionSource '" + name_ +
                               "': no time steps in file '" + file_path + "'.");
  }

  if (file_cells != n_cells)
  {
    return Result<void>::Error(
        ErrorCode::CellCountMismatch,
        "OfflineEmissionSource '" + name_ + "': file '" + file_path +
        "' has " + std::to_string(file_cells) +
        " cells but host expects " + std::to_string(n_cells) + ".");
  }

  // Find the bracketing time indices.  Climatological wrap-around has
  // been removed (plan §D5): out-of-range is a hard error.
  int  left_idx       = 0;
  int  right_idx      = std::min(1, n_times - 1);
  bool found_bracket  = (n_times == 1);

  if (n_times > 1)
  {
    for (int t = 0; t < n_times - 1; ++t)
    {
      if (times[t] <= time_current && time_current <= times[t + 1])
      {
        left_idx      = t;
        right_idx     = t + 1;
        found_bracket = true;
        break;
      }
    }

    if (!found_bracket)
    {
      return Result<void>::Error(
          ErrorCode::TimeOutOfRange,
          "OfflineEmissionSource '" + name_ + "': time " +
          std::to_string(time_current) +
          " falls outside file '" + file_path + "' range [" +
          std::to_string(times.front()) + ", " +
          std::to_string(times.back()) + "].");
    }
  }

  // Read raw inventory data at the bracket times.
  std::vector<Real> raw_left;
  std::vector<Real> raw_right;
  int               n_cells_read = 0;
  reader_->ReadFlux(left_idx,  inventory_species_, raw_left,  n_cells_read);
  reader_->ReadFlux(right_idx, inventory_species_, raw_right, n_cells_read);

  // Apply species mapping: inventory → mechanism.
  std::vector<Real> mapped_left;
  std::vector<Real> mapped_right;
  if (auto r = species_map_.Apply(raw_left,  inventory_species_,
                                  mapped_left,  n_cells); !r)
  {
    return r;
  }
  if (auto r = species_map_.Apply(raw_right, inventory_species_,
                                  mapped_right, n_cells); !r)
  {
    return r;
  }

  const double time_left  = (n_times > 0) ? times[left_idx]  : 0.0;
  const double time_right = (n_times > 1) ? times[right_idx] : time_left + 1.0;

  if (auto r = interpolator_.SetBracket(time_left, time_right,
                                        mapped_left, mapped_right); !r)
  {
    return r;
  }

  brackets_loaded_ = true;
  return Result<void>::Ok();
}

Result<void> OfflineEmissionSource::Update(
    double                    time_current,
    int                       n_cells,
    std::vector<Real>&        flux_out,
    std::vector<std::string>& species_names_out)
{
  if (!brackets_loaded_ || !interpolator_.CoversTime(time_current))
  {
    if (auto r = LoadBrackets(time_current, n_cells); !r)
    {
      return r;
    }
  }

  std::vector<Real> interpolated;
  interpolator_.Interpolate(time_current, interpolated);

  species_names_out = mechanism_species_;
  const int n_mech = static_cast<int>(mechanism_species_.size());

  const std::size_t expected_size =
      static_cast<std::size_t>(n_mech) * n_cells;
  if (!interpolated.empty() && interpolated.size() != expected_size)
  {
    return Result<void>::Error(
        ErrorCode::CellCountMismatch,
        "OfflineEmissionSource '" + name_ + "': interpolated size " +
        std::to_string(interpolated.size()) + " != expected " +
        std::to_string(expected_size) + ".");
  }

  flux_out.resize(expected_size, Real{ 0 });
  for (std::size_t i = 0; i < interpolated.size(); ++i)
  {
    flux_out[i] += interpolated[i];
  }
  return Result<void>::Ok();
}

}  // namespace miem
