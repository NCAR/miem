// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// WebAssembly bridge for the emissions visualizer. It runs the *full* MIEM
// surface-flux pipeline in the browser -- EmissionsBuilder + Emissions::Run +
// OfflineEmissionSource (SpeciesMap::Apply + TemporalInterpolator) + the HEMCO
// aggregation -- per frame. The only step lifted out is netCDF byte-parsing:
// the per-sector slices are extracted server-side by miem_run and fed in via
// InMemoryStore, so this links no netCDF (see reader_factory_wasm.cpp).
//
// The browser loads every sector once, then picks a SpeciesMap on the fly to
// show MIEM aggregating sectors (many -> one), passing one through, or
// splitting by a mass fraction -- all via the real SpeciesMap::Apply.
//
// Usage from JS:
//   miem_load_times(n_cells, n_time, n_sectors, epochs_ptr)   // once
//   miem_load_sector(i, slices_ptr)                           // once per sector
//   miem_build(mode, sector_mask, scale)                      // on selection/mode change
//   ptr = miem_run_at(t)                                      // per frame -> HEAPF64

#include "wasm_in_memory_reader.hpp"

#include <miem/emissions.hpp>
#include <miem/emissions_builder.hpp>
#include <miem/emissions_state.hpp>
#include <miem/source_types.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace miem;

namespace
{
  const std::string kMechanismName = "out";

  int g_n_cells = 0;
  int g_n_sectors = 0;
  std::optional<Emissions> g_module;  // the real built module; rebuilt on selection/mode change
  std::vector<double> g_out;          // reusable n_cells output buffer

  std::string SectorName(int i)
  {
    return "s" + std::to_string(i);
  }

  TemporalInterpolation ModeOf(int mode)
  {
    switch (mode)
    {
      case 1: return TemporalInterpolation::Nearest;
      case 2: return TemporalInterpolation::None;
      default: return TemporalInterpolation::Linear;
    }
  }
}  // namespace

extern "C"
{
  void miem_load_times(int n_cells, int n_time, int n_sectors, const double* epochs)
  {
    g_n_cells = n_cells;
    g_n_sectors = n_sectors;
    InMemoryStore::Instance().SetTimes(n_cells, n_time, epochs);
    g_out.assign(n_cells, 0.0);
  }

  // Register sector i's masked monthly slices as inventory species "s{i}".
  void miem_load_sector(int i, const double* slices)
  {
    InMemoryStore::Instance().SetSpecies(SectorName(i), slices);
  }

  // Build (or rebuild) the real Emissions module. `sector_mask` selects which
  // sectors enter the SpeciesMap (bit i = sector i); `scale` is the per-mapping
  // factor: 1.0 aggregates/passes through, a fraction (<=1) shows splitting.
  void miem_build(int mode, int sector_mask, double scale)
  {
    Source source;
    source.name_ = "viz inventory";
    source.mode_ = SourceMode::Offline;
    source.type_ = SourceType::Anthropogenic;
    source.file_pattern_ = "(in-memory)";
    source.convention_ = "uptempo";  // ignored by the WASM factory
    source.temporal_interpolation_ = ModeOf(mode);
    source.vertical_injection_ = VerticalInjection::Surface;
    for (int i = 0; i < g_n_sectors; ++i)
    {
      if (sector_mask & (1 << i))
      {
        source.species_map_.AddMapping(SectorName(i), kMechanismName, static_cast<Real>(scale));
      }
    }

    g_module.emplace(EmissionsBuilder().SetGridDimensions(g_n_cells, /*n_vert_levels=*/1).AddSource(source).Build());
  }

  // Run the full pipeline at time `t`; returns the mechanism species' surface
  // flux as an n_cells buffer in WASM heap.
  double* miem_run_at(double t)
  {
    if (!g_module)
    {
      return g_out.data();
    }
    const EmissionsState state = g_module->Run(t, /*dt=*/3600.0);
    for (int i = 0; i < g_n_cells; ++i)
    {
      g_out[i] = static_cast<double>(state.surface_flux_(i, kMechanismName));
    }
    return g_out.data();
  }
}
