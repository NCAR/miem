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

#include <optional>
#include <string>
#include <utility>
#include <vector>

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

  const T& value() const& { return *value_; }
  T&       value() &      { return *value_; }
  T        value() &&     { return std::move(*value_); }

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
