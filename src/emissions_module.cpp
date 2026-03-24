#include "miem/emissions_module.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "miem/config.hpp"
#include "miem/flux_converter.hpp"
#include "miem/source_offline.hpp"
#include "miem/util/error.hpp"

namespace miem {

std::vector<std::string> EmissionsModule::QuerySpecies(
    const std::string& config_path) {
  auto config = MIEMConfig::FromYAML(config_path);

  std::set<std::string> all_species;
  for (const auto& src_config : config.sources) {
    OfflineEmissionSource source(src_config);
    auto sp = source.QuerySpecies();
    all_species.insert(sp.begin(), sp.end());
  }

  return {all_species.begin(), all_species.end()};
}

EmissionsModule::EmissionsModule(const std::string& config_path,
                                 int n_cells, int n_vert_levels)
    : n_cells_(n_cells), n_vert_levels_(n_vert_levels) {
  auto config = MIEMConfig::FromYAML(config_path);

  // Create sources and collect species
  std::set<std::string> species_set;
  for (const auto& src_config : config.sources) {
    auto source = std::make_unique<OfflineEmissionSource>(src_config);
    auto sp = source->QuerySpecies();
    species_set.insert(sp.begin(), sp.end());
    sources_.push_back(std::move(source));
  }

  aggregated_species_ = {species_set.begin(), species_set.end()};
}

void EmissionsModule::ResolveHostIndices(
    const std::vector<std::string>& host_species,
    std::vector<int>& indices) const {
  indices.resize(aggregated_species_.size(), -1);

  // Build host species lookup
  std::map<std::string, int> host_map;
  for (int i = 0; i < static_cast<int>(host_species.size()); ++i) {
    host_map[host_species[i]] = i;
  }

  for (int i = 0; i < static_cast<int>(aggregated_species_.size()); ++i) {
    auto it = host_map.find(aggregated_species_[i]);
    if (it != host_map.end()) {
      indices[i] = it->second;
    }
  }
}

EmisState EmissionsModule::Run(double time_current, double dt,
                               const std::vector<Real>& air_density,
                               const std::vector<Real>& layer_thickness) {
  int n_agg = static_cast<int>(aggregated_species_.size());
  EmisState state(n_agg, n_cells_, n_vert_levels_);
  state.species_names = aggregated_species_;

  // Build index map for aggregation
  std::map<std::string, int> agg_idx;
  for (int i = 0; i < n_agg; ++i) {
    agg_idx[aggregated_species_[i]] = i;
  }

  // Collect and aggregate flux from all sources
  for (auto& source : sources_) {
    std::vector<Real> src_flux;
    std::vector<std::string> src_species;
    source->Update(time_current, n_cells_, src_flux, src_species);

    int n_src_sp = static_cast<int>(src_species.size());
    for (int isp = 0; isp < n_src_sp; ++isp) {
      auto it = agg_idx.find(src_species[isp]);
      if (it == agg_idx.end()) continue;
      int dst_idx = it->second;

      for (int ic = 0; ic < n_cells_; ++ic) {
        state.surface_flux[dst_idx * n_cells_ + ic] +=
            src_flux[isp * n_cells_ + ic];
      }
    }
  }

  // Convert surface fluxes to tendencies
  FluxConverter::Convert(state, air_density, layer_thickness);

  return state;
}

}  // namespace miem
