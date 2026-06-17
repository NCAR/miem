// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// An EmissionFileReader that serves slices already in memory instead of
// reading NetCDF. It lets the *entire* real MIEM pipeline -- EmissionsBuilder,
// Emissions::Run, OfflineEmissionSource (SpeciesMap + TemporalInterpolator),
// and the HEMCO aggregation -- run in WASM, with only the file-byte parsing
// (the one genuinely netCDF-bound step) lifted out to the offline miem_run.
//
// The browser fills InMemoryStore once (slices extracted server-side, already
// masked), then the unmodified source pipeline pulls from it through this
// reader exactly as it would from UptempoReader/ECCADReader.

#pragma once

#include "internal/emission_file_reader.hpp"

#include <miem/util/types.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace miem
{

  // Process-wide store the browser populates before building the module.
  struct InMemoryStore
  {
    int n_cells = 0;
    int n_time = 0;
    std::vector<double> epochs;                       // slice times [UTC s]
    std::map<std::string, std::vector<Real>> slices;  // inventory name -> flat [time, cell]

    static InMemoryStore& Instance()
    {
      static InMemoryStore store;
      return store;
    }

    void SetTimes(int n_cells_in, int n_time_in, const double* epochs_in)
    {
      n_cells = n_cells_in;
      n_time = n_time_in;
      epochs.assign(epochs_in, epochs_in + n_time_in);
    }

    void SetSpecies(const std::string& name, const double* flat)
    {
      auto& dst = slices[name];
      dst.assign(static_cast<std::size_t>(n_time) * n_cells, Real{ 0 });
      for (std::size_t i = 0; i < dst.size(); ++i)
      {
        dst[i] = static_cast<Real>(flat[i]);
      }
    }
  };

  class InMemoryReader : public EmissionFileReader
  {
   public:
    void Open(const std::string&) override
    {
      open_ = true;
    }
    void Close() override
    {
      open_ = false;
    }
    bool IsOpen() const override
    {
      return open_;
    }

    int NumTimeSteps() const override
    {
      return InMemoryStore::Instance().n_time;
    }
    int NumCells() const override
    {
      return InMemoryStore::Instance().n_cells;
    }

    std::vector<std::string> QuerySpecies() const override
    {
      std::vector<std::string> names;
      for (const auto& kv : InMemoryStore::Instance().slices)
      {
        names.push_back(kv.first);
      }
      return names;
    }

    std::vector<double> GetTimeValues() const override
    {
      return InMemoryStore::Instance().epochs;
    }

    void ReadFlux(int time_index, const std::vector<std::string>& species_names, std::vector<Real>& flux_out, int& n_cells_out)
        const override
    {
      const auto& store = InMemoryStore::Instance();
      n_cells_out = store.n_cells;
      const int n_sp = static_cast<int>(species_names.size());
      flux_out.assign(static_cast<std::size_t>(n_sp) * store.n_cells, Real{ 0 });
      for (int s = 0; s < n_sp; ++s)
      {
        const auto it = store.slices.find(species_names[s]);
        if (it == store.slices.end())
        {
          continue;
        }
        const Real* row = it->second.data() + static_cast<std::size_t>(time_index) * store.n_cells;
        std::copy(row, row + store.n_cells, flux_out.begin() + static_cast<std::size_t>(s) * store.n_cells);
      }
    }

   private:
    bool open_ = false;
  };

}  // namespace miem
