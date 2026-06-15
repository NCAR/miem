// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include <miem/emissions.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include <miem/flux_converter.hpp>
#include <miem/source_factory.hpp>
#include <miem/util/miem_exception.hpp>

namespace miem {

void Emissions::BuildSources(const std::vector<Source>& sources)
{
  std::set<std::string> species_set;
  for (const auto& src : sources)
  {
    std::unique_ptr<EmissionSource> source;
    try
    {
      source = SourceFactory::Create(src);
    }
    catch (const MiemException& e)
    {
      // Tag the underlying factory error with the source name so the
      // caller sees "source '<name>' failed to construct: <reason>"
      // while preserving the original category/code.
      throw MiemException(e.Category(), e.Code(),
                          "source '" + src.name_ +
                          "' failed to construct: " + e.what());
    }
    const auto sp = source->QuerySpecies();
    species_set.insert(sp.begin(), sp.end());
    sources_.push_back(SourceEntry{
        std::move(source),
        src.category_,
        src.hierarchy_,
        src.sector_,
        src.scaling_factor_,
    });
  }

  aggregated_species_.assign(species_set.begin(), species_set.end());
}

// Constructed only by EmissionsBuilder::Build(), which has already
// validated the configuration.  A source-factory rejection here would mean
// Validate() missed it, so we let it propagate (tagged with the source
// name) rather than swallow it from a constructor.
Emissions::Emissions(const std::vector<Source>& sources,
                     int                        n_cells,
                     int                        n_vert_levels)
    : n_cells_(n_cells), n_vert_levels_(n_vert_levels)
{
  BuildSources(sources);  // throws MiemException on factory failure
}

void Emissions::ResolveHostIndices(
    const std::vector<std::string>& host_species,
    std::vector<int>&               indices) const
{
  indices.assign(aggregated_species_.size(), -1);

  std::map<std::string, int> host_map;
  for (int i = 0; i < static_cast<int>(host_species.size()); ++i)
  {
    host_map[host_species[i]] = i;
  }

  for (int i = 0; i < static_cast<int>(aggregated_species_.size()); ++i)
  {
    auto it = host_map.find(aggregated_species_[i]);
    if (it != host_map.end())
    {
      indices[i] = it->second;
    }
  }
}

EmissionsState Emissions::Run(double sim_time_sec, double /*dt_sec*/)
{
  const int n_agg = static_cast<int>(aggregated_species_.size());

  EmissionsState state(n_agg, n_cells_, n_vert_levels_);
  state.species_names_ = aggregated_species_;
  state.surface_flux_.SetSpecies(aggregated_species_);

  std::map<std::string, int> agg_idx;
  for (int i = 0; i < n_agg; ++i)
  {
    agg_idx[aggregated_species_[i]] = i;
  }

  const std::size_t flux_size = static_cast<std::size_t>(n_agg) * n_cells_;

  // Per-source flux + metadata for HEMCO-style aggregation.
  struct SourceFlux
  {
    int               category_;
    int               hierarchy_;
    std::string       sector_;
    std::vector<Real> flux_;  // (n_agg * n_cells)
  };

  std::vector<SourceFlux> source_fluxes;
  source_fluxes.reserve(sources_.size());

  // EmissionSource::Update throws MiemException on failure; it propagates
  // directly to the caller (and onward to musica::HandleErrors at the C
  // boundary), preserving the original IO/Validation category and code.
  for (auto& entry : sources_)
  {
    std::vector<Real>        src_flux;
    std::vector<std::string> src_species;
    entry.source_->Update(sim_time_sec, n_cells_, src_flux, src_species);

    std::vector<Real> mapped(flux_size, Real{ 0 });
    const int n_src_sp = static_cast<int>(src_species.size());
    for (int isp = 0; isp < n_src_sp; ++isp)
    {
      const auto it = agg_idx.find(src_species[isp]);
      if (it == agg_idx.end())
      {
        continue;
      }
      const int dst_idx = it->second;
      for (int ic = 0; ic < n_cells_; ++ic)
      {
        mapped[static_cast<std::size_t>(dst_idx) * n_cells_ + ic] =
            src_flux[static_cast<std::size_t>(isp) * n_cells_ + ic] *
            entry.scaling_factor_;
      }
    }

    source_fluxes.push_back(SourceFlux{
        entry.category_,
        entry.hierarchy_,
        entry.sector_,
        std::move(mapped),
    });
  }

  // HEMCO-style aggregation: within each category the highest-hierarchy
  // source wins per (species, cell); categories are summed.
  std::set<int> categories;
  for (const auto& sf : source_fluxes)
  {
    categories.insert(sf.category_);
  }

  for (int cat : categories)
  {
    std::vector<const SourceFlux*> cat_sources;
    for (const auto& sf : source_fluxes)
    {
      if (sf.category_ == cat)
      {
        cat_sources.push_back(&sf);
      }
    }
    std::sort(cat_sources.begin(), cat_sources.end(),
              [](const SourceFlux* a, const SourceFlux* b) {
                return a->hierarchy_ > b->hierarchy_;
              });

    for (int isp = 0; isp < n_agg; ++isp)
    {
      for (int ic = 0; ic < n_cells_; ++ic)
      {
        const std::size_t idx =
            static_cast<std::size_t>(isp) * n_cells_ + ic;
        for (const auto* src : cat_sources)
        {
          if (src->flux_[idx] != Real{ 0 })
          {
            state.surface_flux_.At(isp, ic) += src->flux_[idx];
            break;
          }
        }
      }
    }
  }

  // Per-sector diagnostic fluxes.  Same-sector sources sum.
  for (const auto& sf : source_fluxes)
  {
    if (sf.sector_.empty())
    {
      continue;
    }
    auto it = state.sector_fluxes_.find(sf.sector_);
    if (it == state.sector_fluxes_.end())
    {
      EmissionsArray fa(n_agg, n_cells_);
      fa.SetSpecies(aggregated_species_);
      auto& raw = fa.raw();
      raw = sf.flux_;
      state.sector_fluxes_.emplace(sf.sector_, std::move(fa));
      state.sector_names_.push_back(sf.sector_);
    }
    else
    {
      auto& raw = it->second.raw();
      for (std::size_t i = 0; i < raw.size(); ++i)
      {
        raw[i] += sf.flux_[i];
      }
    }
  }

  return state;
}

EmissionsState Emissions::Run(double      sim_time_sec,
                              double      dt_sec,
                              const Real* air_density,
                              const Real* layer_thickness,
                              int         n_atm_elements)
{
  EmissionsState state = Run(sim_time_sec, dt_sec);  // throws on failure
  FluxConverter::Apply(state, air_density, layer_thickness, n_atm_elements);
  return state;
}

}  // namespace miem
