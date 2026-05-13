// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// `miem::Result<T>` — public-boundary error container.  Modeled on
// `mechanism_configuration::ParserResult<T>`: `operator bool`, `errors`
// vector of `{ErrorCode, std::string}`, and (for non-void T) an optional
// `value`.  Kernels may throw `MIEMError` internally; every public
// `Result`-returning function and every C-API entry catches at the
// boundary and converts to `Result::Error{…}`.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// `MIEM_ASSERT` — release-mode-active assertion used at boundaries where
// silently dereferencing an empty `std::optional` would be undefined
// behavior.  Public C++ does not throw, so a hard abort with a
// descriptive message is the right escalation when callers ignore the
// `bool` operator and grab `.value()` on an empty `Result`.
#ifndef MIEM_ASSERT
#  define MIEM_ASSERT(cond, msg)                                              \
    do {                                                                      \
      if (!(cond)) {                                                          \
        std::fprintf(stderr,                                                  \
                     "MIEM_ASSERT failed at %s:%d: %s -- %s\n",               \
                     __FILE__, __LINE__, #cond, msg);                         \
        std::abort();                                                         \
      }                                                                       \
    } while (0)
#endif

namespace miem {

enum class ErrorCode
{
  Ok,
  ConfigInvalid,
  UnsupportedRegriddingType,
  UnknownConvention,
  OnlineSourcesNotSupported,
  UnsupportedVerticalInjection,
  DuplicateCategoryHierarchy,
  FileNotFound,
  NetCDFError,
  SpeciesMapScalingExceedsOne,
  CellCountMismatch,
  TimeOutOfRange,
  MassConservationViolation,
  UnsupportedCalendar,
  InvalidTimeUnits,
  UnknownSourceType,
  UnknownSector,
  InternalError,
};

struct ErrorEntry
{
  ErrorCode   code_;
  std::string message_;
};

template <typename T>
class Result
{
 public:
  Result() = default;

  static Result Ok(T value)
  {
    Result r;
    r.value_ = std::move(value);
    return r;
  }

  static Result Error(ErrorCode code, std::string message)
  {
    Result r;
    r.errors_.push_back({ code, std::move(message) });
    return r;
  }

  // Construct from an existing errors vector (used by chained validators).
  static Result Errors(std::vector<ErrorEntry> errors)
  {
    Result r;
    r.errors_ = std::move(errors);
    return r;
  }

  explicit operator bool() const noexcept { return errors_.empty(); }

  const std::vector<ErrorEntry>& errors() const noexcept { return errors_; }
  std::vector<ErrorEntry>&       errors() noexcept       { return errors_; }

  const T& value() const&
  {
    MIEM_ASSERT(value_.has_value(),
                "Result<T>::value() called on a Result with no value -- "
                "check operator bool() / has_value() first.");
    return *value_;
  }
  T& value() &
  {
    MIEM_ASSERT(value_.has_value(),
                "Result<T>::value() called on a Result with no value -- "
                "check operator bool() / has_value() first.");
    return *value_;
  }
  T value() &&
  {
    MIEM_ASSERT(value_.has_value(),
                "Result<T>::value() called on a Result with no value -- "
                "check operator bool() / has_value() first.");
    return std::move(*value_);
  }

  bool has_value() const noexcept { return value_.has_value(); }

  void AddError(ErrorCode code, std::string message)
  {
    errors_.push_back({ code, std::move(message) });
  }

 private:
  std::optional<T>        value_;
  std::vector<ErrorEntry> errors_;
};

// Specialization for void — no value, just success / error list.
template <>
class Result<void>
{
 public:
  Result() = default;

  static Result Ok()
  {
    return Result{};
  }

  static Result Error(ErrorCode code, std::string message)
  {
    Result r;
    r.errors_.push_back({ code, std::move(message) });
    return r;
  }

  static Result Errors(std::vector<ErrorEntry> errors)
  {
    Result r;
    r.errors_ = std::move(errors);
    return r;
  }

  explicit operator bool() const noexcept { return errors_.empty(); }

  const std::vector<ErrorEntry>& errors() const noexcept { return errors_; }
  std::vector<ErrorEntry>&       errors() noexcept       { return errors_; }

  void AddError(ErrorCode code, std::string message)
  {
    errors_.push_back({ code, std::move(message) });
  }

 private:
  std::vector<ErrorEntry> errors_;
};

}  // namespace miem
