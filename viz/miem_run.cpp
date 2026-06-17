// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Local, throwaway driver for the emissions visualizer (viz/).  NOT part of
// MIEM's build or install surface -- it is compiled on demand by
// build_and_run.sh against the prebuilt libmiem.a.
//
// It runs the real Emissions pipeline over an UPTEMPO on-mesh file and dumps
// the per-cell surface flux of one mechanism species to a flat binary file
// (row-major [time, cell], little-endian float64).  The Python converter
// (make_zarr.py) reads that back as the "after MIEM" field to compare against
// the raw inventory "before".
//
// Usage:
//   miem_run <input.nc> <inventory_var> <mech_species> <n_cells> <interp> \
//            <out.bin> <epoch0> [epoch1 ...]
//
// <interp> is one of linear | nearest | none -- the temporal interpolation
// MIEM applies between the file's monthly slices. The epoch arguments are the
// per-time-step UTC seconds the host would pass to Run(); make_zarr.py derives
// them from the file's xtime stamps exactly as the reader does, and may pass a
// finer (e.g. daily) cadence than the file's slices to exercise interpolation.

#include <miem/emissions.hpp>
#include <miem/emissions_builder.hpp>
#include <miem/emissions_state.hpp>
#include <miem/source_types.hpp>
#include <miem/util/miem_exception.hpp>
#include <miem/util/types.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
  if (argc < 8)
  {
    std::fprintf(
        stderr,
        "usage: %s <input.nc> <inventory_var> <mech_species> <n_cells> "
        "<linear|nearest|none> <out.bin> <epoch0> [epoch1 ...]\n",
        argv[0]);
    return 2;
  }

  const std::string input_path = argv[1];
  const std::string inventory_var = argv[2];
  const std::string mech_species = argv[3];
  const int n_cells = std::atoi(argv[4]);
  const std::string interp = argv[5];
  const std::string out_path = argv[6];

  std::vector<double> epochs;
  for (int i = 7; i < argc; ++i)
  {
    epochs.push_back(std::atof(argv[i]));
  }

  using namespace miem;

  TemporalInterpolation interp_mode = TemporalInterpolation::Linear;
  if (interp == "nearest")
  {
    interp_mode = TemporalInterpolation::Nearest;
  }
  else if (interp == "none")
  {
    interp_mode = TemporalInterpolation::None;
  }
  else if (interp != "linear")
  {
    std::fprintf(stderr, "miem_run: unknown interp '%s' (use linear|nearest|none)\n", interp.c_str());
    return 2;
  }

  Source source;
  source.name_ = "viz inventory";
  source.mode_ = SourceMode::Offline;
  source.type_ = SourceType::Anthropogenic;
  source.file_pattern_ = input_path;
  source.convention_ = "uptempo";
  source.temporal_interpolation_ = interp_mode;
  source.vertical_injection_ = VerticalInjection::Surface;
  source.sector_ = "anthropogenic";
  source.species_map_.AddMapping(inventory_var, mech_species, 1.0);

  Emissions module = EmissionsBuilder().SetGridDimensions(n_cells, /*n_vert_levels=*/1).AddSource(source).Build();

  std::ofstream out(out_path, std::ios::binary);
  if (!out)
  {
    std::fprintf(stderr, "miem_run: cannot open output '%s'\n", out_path.c_str());
    return 1;
  }

  std::vector<double> row(static_cast<std::size_t>(n_cells), 0.0);
  for (std::size_t t = 0; t < epochs.size(); ++t)
  {
    try
    {
      const EmissionsState state = module.Run(epochs[t], /*dt=*/3600.0);
      for (int ic = 0; ic < n_cells; ++ic)
      {
        row[static_cast<std::size_t>(ic)] = static_cast<double>(state.surface_flux_(ic, mech_species));
      }
    }
    catch (const MiemException& e)
    {
      // A single out-of-range step should not sink the whole run; emit zeros
      // for it and keep going, but make the gap visible on stderr.
      std::fprintf(stderr, "miem_run: step %zu (epoch %.0f) failed: %s\n", t, epochs[t], e.what());
      std::fill(row.begin(), row.end(), 0.0);
    }
    out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size() * sizeof(double)));
  }

  std::fprintf(stderr, "miem_run: wrote %zu time steps x %d cells to %s\n", epochs.size(), n_cells, out_path.c_str());
  return 0;
}
