// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Translation helpers between `miem::Result<T>` (and bare exceptions)
// and `MIEM_Error`.  Used by every C-API entry to keep the boundary
// uniform.
#pragma once

#include <cstring>
#include <exception>
#include <string>
#include <utility>

#include "miem/miem_c.h"
#include "miem/util/error.hpp"
#include "miem/util/result.hpp"

namespace miem {
namespace c_api {

inline const char* ErrorCodeName(ErrorCode code)
{
  switch (code)
  {
    case ErrorCode::Ok:                              return "Ok";
    case ErrorCode::ConfigInvalid:                   return "ConfigInvalid";
    case ErrorCode::UnsupportedRegriddingType:       return "UnsupportedRegriddingType";
    case ErrorCode::UnknownConvention:               return "UnknownConvention";
    case ErrorCode::OnlineSourcesNotSupported:       return "OnlineSourcesNotSupported";
    case ErrorCode::UnsupportedVerticalInjection:    return "UnsupportedVerticalInjection";
    case ErrorCode::DuplicateCategoryHierarchy:      return "DuplicateCategoryHierarchy";
    case ErrorCode::FileNotFound:                    return "FileNotFound";
    case ErrorCode::NetCDFError:                     return "NetCDFError";
    case ErrorCode::SpeciesMapScalingExceedsOne:     return "SpeciesMapScalingExceedsOne";
    case ErrorCode::CellCountMismatch:               return "CellCountMismatch";
    case ErrorCode::TimeOutOfRange:                  return "TimeOutOfRange";
    case ErrorCode::MassConservationViolation:       return "MassConservationViolation";
    case ErrorCode::UnsupportedCalendar:             return "UnsupportedCalendar";
    case ErrorCode::InvalidTimeUnits:                return "InvalidTimeUnits";
    case ErrorCode::UnknownSourceType:               return "UnknownSourceType";
    case ErrorCode::UnknownSector:                   return "UnknownSector";
    case ErrorCode::InternalError:                   return "InternalError";
  }
  return "InternalError";
}

inline void ClearError(MIEM_Error* err)
{
  if (err)
  {
    std::memset(err, 0, sizeof(MIEM_Error));
  }
}

inline void SetError(MIEM_Error* err,
                     int         code,
                     const char* category,
                     const char* message)
{
  if (!err) return;
  err->code = code;
  std::strncpy(err->category, category ? category : "",
               sizeof(err->category) - 1);
  err->category[sizeof(err->category) - 1] = '\0';
  std::strncpy(err->message, message ? message : "",
               sizeof(err->message) - 1);
  err->message[sizeof(err->message) - 1] = '\0';
}

inline void SetErrorFromEntry(MIEM_Error* err, const ErrorEntry& e)
{
  SetError(err, static_cast<int>(e.code_), ErrorCodeName(e.code_),
           e.message_.c_str());
}

// Map an exception's category string (set by the MIEMError-derived
// constructor) onto the closest semantic ErrorCode.  Returning the
// matching enum lets the C-API surface "this was an IO error" vs
// "this was a config error" via err->code, not just err->category --
// downstream Fortran/C consumers may switch on the integer.
//
// We reuse existing ErrorCode values rather than proliferate new ones:
//   "Configuration" / "Validation" -> ConfigInvalid
//   "IO"                           -> NetCDFError  (covers all I/O
//                                      failures from ECCADReader; the
//                                      message string carries the
//                                      file path / netCDF detail)
//   "Species"                      -> ConfigInvalid (species-mapping
//                                      issues are configuration data)
//   anything else                  -> InternalError
inline ErrorCode ErrorCodeFromCategory(const std::string& category)
{
  if (category == "Configuration") return ErrorCode::ConfigInvalid;
  if (category == "IO")            return ErrorCode::NetCDFError;
  if (category == "Species")       return ErrorCode::ConfigInvalid;
  if (category == "Validation")    return ErrorCode::ConfigInvalid;
  return ErrorCode::InternalError;
}

// Translate a `Result<T>` into success/failure for the C API.  On
// success returns true (and the caller pulls the value out via
// `r.value()`).  On failure populates `err` from the first error.
template <typename T>
bool UnwrapOrSet(Result<T>& r, MIEM_Error* err)
{
  if (r)
  {
    ClearError(err);
    return true;
  }
  if (!r.errors().empty())
  {
    SetErrorFromEntry(err, r.errors().front());
  }
  else
  {
    SetError(err, static_cast<int>(ErrorCode::InternalError),
             "InternalError", "Unknown failure");
  }
  return false;
}

// Run `func` and translate any thrown `MIEMError` (or other exception)
// into an `MIEM_Error`.  Used to wrap kernel calls inside C-API entries.
//
// Code mapping: every thrown MIEMError carries a category string
// ("Configuration", "IO", ...).  ErrorCodeFromCategory translates that
// to the canonical ErrorCode enum, which is what the consumer reads
// via err->code -- not the previous all-collapse-to-InternalError.
template <typename F>
void HandleErrors(MIEM_Error* err, F&& func)
{
  ClearError(err);
  try
  {
    func();
  }
  catch (const ::miem::MIEMError& e)
  {
    const ErrorCode code = ErrorCodeFromCategory(e.Category());
    SetError(err, static_cast<int>(code), e.Category().c_str(), e.what());
  }
  catch (const std::exception& e)
  {
    SetError(err, static_cast<int>(ErrorCode::InternalError),
             "Exception", e.what());
  }
  catch (...)
  {
    SetError(err, static_cast<int>(ErrorCode::InternalError),
             "Unknown", "Unknown error occurred");
  }
}

}  // namespace c_api
}  // namespace miem
