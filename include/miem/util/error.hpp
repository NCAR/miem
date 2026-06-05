// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Internal exception hierarchy.  Public C++ API returns `miem::Result<T>`
// (defined in `result.hpp`); kernels may throw these internally to
// short-circuit deep call stacks, and a boundary-layer try/catch at every
// public entry converts them to `Result::Error{code, message}`.
#pragma once

#include <stdexcept>
#include <string>

namespace miem {

class MIEMError : public std::runtime_error
{
 public:
  MIEMError(const std::string& category, const std::string& message)
      : std::runtime_error(message), category_(category)
  {
  }

  const std::string& Category() const { return category_; }

 private:
  std::string category_;
};

class ConfigError : public MIEMError
{
 public:
  explicit ConfigError(const std::string& message)
      : MIEMError("Configuration", message) {}
};

class IOError : public MIEMError
{
 public:
  explicit IOError(const std::string& message)
      : MIEMError("IO", message) {}
};

class SpeciesError : public MIEMError
{
 public:
  explicit SpeciesError(const std::string& message)
      : MIEMError("Species", message) {}
};

class ValidationError : public MIEMError
{
 public:
  explicit ValidationError(const std::string& message)
      : MIEMError("Validation", message) {}
};

}  // namespace miem
