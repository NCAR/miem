// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// WASM-only replacement for src/reader_factory.cpp. The real factory
// instantiates the NetCDF-backed readers (UptempoReader/ECCADReader), which
// would drag <netcdf.h> (and HDF5) into the WASM build. Here every convention
// resolves to the in-memory reader instead, so the rest of the source pipeline
// links and runs unchanged. Link this *instead of* src/reader_factory.cpp.

#include "internal/reader_factory.hpp"

#include "wasm_in_memory_reader.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace miem
{

  namespace
  {
    std::string ToLower(std::string s)
    {
      std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
      return s;
    }
  }  // namespace

  // Same supported set as the real factory, so EmissionsBuilder::Validate
  // accepts the same conventions; the data just arrives in memory.
  bool IsSupportedConvention(const std::string& convention)
  {
    const std::string key = ToLower(convention);
    return key == "eccad" || key == "uptempo";
  }

  std::unique_ptr<EmissionFileReader> MakeEmissionFileReader(const std::string& /*convention*/)
  {
    return std::make_unique<InMemoryReader>();
  }

}  // namespace miem
