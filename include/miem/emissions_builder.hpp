// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `EmissionsBuilder` assembles a set of `Source` descriptions into a
// runtime `Emissions` module, mirroring micm's CpuSolverBuilder.  All v1
// invariant checks run inside `Build()`, which throws `miem::MiemException`
// on the first violation -- there is no separate validation pass, and the
// `Source` domain objects carry none (validation upstream lives in
// MechanismConfiguration, exactly as it does for micm).
//
//   auto emissions = EmissionsBuilder()
//                        .SetGridDimensions(n_cells, n_vert_levels)
//                        .AddSource(source)
//                        .Build();
#pragma once

#include <utility>
#include <vector>

#include <miem/source_types.hpp>
#include <miem/emissions.hpp>

namespace miem {

class EmissionsBuilder
{
 public:
  // Host grid dimensions.  Required before Build().
  EmissionsBuilder& SetGridDimensions(int n_cells, int n_vert_levels)
  {
    n_cells_       = n_cells;
    n_vert_levels_ = n_vert_levels;
    return *this;
  }

  // Regridding spec.  v1 supports only RegriddingType::None (the default);
  // Build() rejects anything else.
  EmissionsBuilder& SetRegridding(const Regridding& regridding)
  {
    regridding_ = regridding;
    return *this;
  }

  // Append one source description.
  EmissionsBuilder& AddSource(const Source& source)
  {
    sources_.push_back(source);
    return *this;
  }

  EmissionsBuilder& AddSource(Source&& source)
  {
    sources_.push_back(std::move(source));
    return *this;
  }

  // Replace the source list wholesale.
  EmissionsBuilder& SetSources(std::vector<Source> sources)
  {
    sources_ = std::move(sources);
    return *this;
  }

  // Validate the assembled configuration and build the runtime module.
  // Throws miem::MiemException on the first failed invariant or a source
  // the factory rejects.  Returns the module by value.
  Emissions Build() const;

 private:
  // Throws MiemException on the first violated v1 invariant.
  void Validate() const;

  Regridding          regridding_{};
  std::vector<Source> sources_{};
  int                 n_cells_       = 0;
  int                 n_vert_levels_ = 0;
};

}  // namespace miem
