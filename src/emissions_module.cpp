#include "miem/emissions_module.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "miem/config.hpp"
#include "miem/flux_converter.hpp"
#include "miem/source_factory.hpp"
#include "miem/util/error.hpp"

namespace miem {

std::vector<std::string> EmissionsModule::QuerySpecies(
    const std::string& config_path) {
  return QuerySpecies(MIEMConfig::FromYAML(config_path));
}

std::vector<std::string> EmissionsModule::QuerySpecies(
    const MIEMConfig& config) {
  std::set<std::string> all_species;
  for (const auto& src_config : config.sources) {
    auto source = SourceFactory::Create(src_config);
    auto sp = source->QuerySpecies();
    all_species.insert(sp.begin(), sp.end());
  }

  return {all_species.begin(), all_species.end()};
}

EmissionsModule::EmissionsModule(const std::string& config_path,
                                 int n_cells, int n_vert_levels)
    : n_cells_(n_cells), n_vert_levels_(n_vert_levels) {
  auto config = MIEMConfig::FromYAML(config_path);

  // Create sources via factory and collect species
  std::set<std::string> species_set;
  for (const auto& src_config : config.sources) {
    auto source = SourceFactory::Create(src_config);
    auto sp = source->QuerySpecies();
    species_set.insert(sp.begin(), sp.end());
    sources_.push_back({
        std::move(source),
        src_config.category,
        src_config.hierarchy,
        src_config.sector,
        src_config.scaling_factor
    });
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

EmisState EmissionsModule::Run(double time_current,
                               const Real* air_density,
                               const Real* layer_thickness,
                               int n_atm_elements) {
  int n_agg = static_cast<int>(aggregated_species_.size());
  EmisState state(n_agg, n_cells_, n_vert_levels_);
  state.species_names = aggregated_species_;

  // Build index map for aggregation
  std::map<std::string, int> agg_idx;
  for (int i = 0; i < n_agg; ++i) {
    agg_idx[aggregated_species_[i]] = i;
  }

  size_t flux_size = static_cast<size_t>(n_agg) * n_cells_;

  // Per-source flux tagged with metadata for category/hierarchy resolution.
  // Structure: for each source, store its scaled flux.
  struct SourceFlux {
    int category;
    int hierarchy;
    std::string sector;
    std::vector<Real> flux;  // (n_agg * n_cells)
  };

  std::vector<SourceFlux> source_fluxes;
  source_fluxes.reserve(sources_.size());

  for (auto& entry : sources_) {
    std::vector<Real> src_flux;
    std::vector<std::string> src_species;
    entry.source->Update(time_current, n_cells_, src_flux, src_species);

    // Map source species into aggregated species space and apply scaling
    std::vector<Real> mapped(flux_size, 0.0);
    int n_src_sp = static_cast<int>(src_species.size());
    for (int isp = 0; isp < n_src_sp; ++isp) {
      auto it = agg_idx.find(src_species[isp]);
      if (it == agg_idx.end()) continue;
      int dst_idx = it->second;

      for (int ic = 0; ic < n_cells_; ++ic) {
        mapped[dst_idx * n_cells_ + ic] =
            src_flux[isp * n_cells_ + ic] * entry.scaling_factor;
      }
    }

    source_fluxes.push_back({
        entry.category,
        entry.hierarchy,
        entry.sector,
        std::move(mapped)
    });
  }

  // Collect distinct categories
  std::set<int> categories;
  for (const auto& sf : source_fluxes) {
    categories.insert(sf.category);
  }

  // Category/hierarchy aggregation:
  //   1. Within each category, for each (species, cell), take the flux from
  //      the highest-hierarchy source that has non-zero flux.
  //   2. Sum across categories.
  for (int cat : categories) {
    // Gather sources in this category, sorted by hierarchy descending
    std::vector<const SourceFlux*> cat_sources;
    for (const auto& sf : source_fluxes) {
      if (sf.category == cat) {
        cat_sources.push_back(&sf);
      }
    }
    std::sort(cat_sources.begin(), cat_sources.end(),
              [](const SourceFlux* a, const SourceFlux* b) {
                return a->hierarchy > b->hierarchy;
              });

    // For each species/cell, take the first (highest hierarchy) non-zero value
    for (int isp = 0; isp < n_agg; ++isp) {
      for (int ic = 0; ic < n_cells_; ++ic) {
        size_t idx = static_cast<size_t>(isp) * n_cells_ + ic;
        for (const auto* src : cat_sources) {
          if (src->flux[idx] != 0.0) {
            state.surface_flux[idx] += src->flux[idx];
            break;
          }
        }
      }
    }
  }

  // Populate per-sector fluxes for diagnostics
  for (const auto& sf : source_fluxes) {
    if (sf.sector.empty()) continue;

    auto it = state.sector_fluxes.find(sf.sector);
    if (it == state.sector_fluxes.end()) {
      state.sector_fluxes[sf.sector] = sf.flux;
      state.sector_names.push_back(sf.sector);
    } else {
      // Multiple sources with same sector label: sum them
      for (size_t i = 0; i < flux_size; ++i) {
        it->second[i] += sf.flux[i];
      }
    }
  }

  // Convert surface fluxes to tendencies
  FluxConverter::Convert(state, air_density, layer_thickness, n_atm_elements);

  return state;
}

}  // namespace miem
