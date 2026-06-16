// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0

#include "internal/reader_factory.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include "internal/eccad_reader.hpp"
#include "internal/emission_file_reader.hpp"
#include "internal/uptempo_reader.hpp"
#include <miem/util/error.hpp>
#include <miem/util/miem_exception.hpp>

namespace miem {

namespace {

std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

}  // namespace

bool IsSupportedConvention(const std::string& convention)
{
  const std::string key = ToLower(convention);
  return key == "eccad" || key == "uptempo";
}

std::unique_ptr<EmissionFileReader> MakeEmissionFileReader(
    const std::string& convention)
{
  const std::string key = ToLower(convention);
  if (key == "eccad")
  {
    return std::make_unique<ECCADReader>();
  }
  if (key == "uptempo")
  {
    return std::make_unique<UptempoReader>();
  }

  throw MiemException(
      MIEM_ERROR_CATEGORY_CONFIGURATION,
      MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION,
      "unknown inventory convention '" + convention +
      "' — v1 supports 'eccad' and 'uptempo'.");
}

}  // namespace miem
