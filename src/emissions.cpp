// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "miem/emissions.hpp"

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "miem/flux_converter.hpp"
#include "miem/source_factory.hpp"
#include "miem/util/error.hpp"

namespace miem {

Result<void> Emissions::BuildSources(const EmissionsConfig& cfg)
{
  std::vector<ErrorEntry> errors;

  std::set<std::string> species_set;
  for (const auto& src_cfg : cfg.sources_)
  {
    auto factory = SourceFactory::Create(src_cfg);
    if (!factory)
    {
      // Tag each underlying factory error with the source name so the
      // caller sees "source '<name>' failed: <reason>" rather than a
      // bare count.
      for (const auto& e : factory.errors())
      {
        errors.push_back({
            e.code_,
            "source '" + src_cfg.name_ + "' failed to construct: " +
                e.message_,
        });
      }
      continue;
    }
    std::unique_ptr<EmissionSource> source = std::move(factory).value();
    const auto sp = source->QuerySpecies();
    species_set.insert(sp.begin(), sp.end());
    sources_.push_back(SourceEntry{
        std::move(source),
        src_cfg.category_,
        src_cfg.hierarchy_,
        src_cfg.sector_,
        src_cfg.scaling_factor_,
    });
  }

  aggregated_species_.assign(species_set.begin(), species_set.end());
  if (!errors.empty())
  {
    return Result<void>::Errors(std::move(errors));
  }
  return Result<void>::Ok();
}

Emissions::Emissions(const EmissionsConfig& cfg,
                                 int               n_cells,
                                 int               n_vert_levels)
    : n_cells_(n_cells), n_vert_levels_(n_vert_levels)
{
  // Defense in depth: assert the regridding precondition that
  // EmissionsConfig::Validate() already enforces (plan §D6).
  assert(cfg.regridding_.type_ == RegriddingType::None &&
         "Emissions: regridding != None — call EmissionsConfig::Validate()");

  // Construction errors are silently discarded here — callers should
  // prefer the `Create` factory which surfaces them.  Source-factory
  // failures are an oversight in pre-validation (Validate should have
  // caught them), so we leave sources_ partially populated rather than
  // throw from a constructor.
  (void)BuildSources(cfg);
}

Result<std::unique_ptr<Emissions>>
Emissions::Create(const EmissionsConfig& cfg,
                        int               n_cells,
                        int               n_vert_levels)
{
  if (auto v = cfg.Validate(); !v)
  {
    return Result<std::unique_ptr<Emissions>>::Errors(v.errors());
  }

  // Construct an empty shell, then invoke BuildSources directly so we
  // get the structured per-source error list.  The legacy constructor
  // path also calls BuildSources but silently discards the errors;
  // here we want them.
  auto module = std::unique_ptr<Emissions>(
      new Emissions(n_cells, n_vert_levels));

  Result<void> built = module->BuildSources(cfg);
  if (!built)
  {
    return Result<std::unique_ptr<Emissions>>::Errors(built.errors());
  }
  return Result<std::unique_ptr<Emissions>>::Ok(std::move(module));
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

Result<EmissionsState> Emissions::Run(double sim_time_sec, double /*dt_sec*/)
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

  try
  {
    for (auto& entry : sources_)
    {
      std::vector<Real>        src_flux;
      std::vector<std::string> src_species;
      auto r = entry.source_->Update(sim_time_sec, n_cells_,
                                     src_flux, src_species);
      if (!r)
      {
        return Result<EmissionsState>::Errors(r.errors());
      }

      std::vector<Real> mapped(flux_size, Real{ 0 });
      const int n_src_sp = static_cast<int>(src_species.size());
      for (int isp = 0; isp < n_src_sp; ++isp)
      {
        const auto it = agg_idx.find(src_species[isp]);
        if (it == agg_idx.end()) continue;
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
  }
  catch (const MIEMError& e)
  {
    return Result<EmissionsState>::Error(ErrorCode::InternalError,
                                    std::string("Emissions::Run: ") +
                                    e.what());
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
      if (sf.category_ == cat) cat_sources.push_back(&sf);
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
    if (sf.sector_.empty()) continue;
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

  return Result<EmissionsState>::Ok(std::move(state));
}

Result<EmissionsState> Emissions::Run(double      sim_time_sec,
                                       double      dt_sec,
                                       const Real* air_density,
                                       const Real* layer_thickness,
                                       int         n_atm_elements)
{
  auto run_result = Run(sim_time_sec, dt_sec);
  if (!run_result)
  {
    return run_result;
  }
  EmissionsState state = std::move(run_result).value();

  if (auto conv = FluxConverter::Apply(state, air_density,
                                       layer_thickness, n_atm_elements); !conv)
  {
    return Result<EmissionsState>::Errors(conv.errors());
  }
  return Result<EmissionsState>::Ok(std::move(state));
}

}  // namespace miem
