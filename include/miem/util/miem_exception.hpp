// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdexcept>
#include <string>

namespace miem {

/// @brief The single MIEM exception type, modeled on
///        @c micm::MicmException.
///
/// Carries a string-literal @c category_ and an integer @c code_ (see
/// error.hpp for the MIEM_ERROR_CATEGORY_* / MIEM_*_ERROR_CODE_* macros).
/// Every public MIEM entry point throws this on failure; at the C/Fortran
/// boundary @c musica::HandleErrors() catches it and converts it to a
/// MUSICA @c Error struct, preserving the category and code. This mirrors
/// how MICM throws @c micm::MicmException.
struct MiemException : public std::runtime_error
{
  const char* category_;  ///< A MIEM_ERROR_CATEGORY_* string literal.
  int         code_;      ///< A MIEM_*_ERROR_CODE_* value.

  MiemException(const char* category, int code, const std::string& message)
      : std::runtime_error(message), category_(category), code_(code)
  {
  }

  const char* Category() const noexcept { return category_; }
  int         Code()     const noexcept { return code_; }
};

}  // namespace miem
