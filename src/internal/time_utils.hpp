// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Portable UTC <-> Unix-epoch conversions built on std::chrono (C++20),
// replacing the platform-specific gmtime_r/gmtime_s and timegm/_mkgmtime.
// std::chrono::system_clock is Unix-epoch / UTC, so no timezone handling
// is needed.
#pragma once

#include <chrono>
#include <ctime>

namespace miem {

// UTC broken-down calendar time -> seconds since the Unix epoch.
inline std::time_t TimeFromUtcTm(const std::tm& tm)
{
  using namespace std::chrono;
  const year_month_day ymd{year{tm.tm_year + 1900} /
                           month{static_cast<unsigned>(tm.tm_mon + 1)} /
                           day{static_cast<unsigned>(tm.tm_mday)}};
  const seconds since_epoch =
      duration_cast<seconds>(sys_days{ymd}.time_since_epoch()) +
      hours{tm.tm_hour} + minutes{tm.tm_min} + seconds{tm.tm_sec};
  return static_cast<std::time_t>(since_epoch.count());
}

// Seconds since the Unix epoch -> UTC broken-down calendar time.
inline std::tm UtcTmFromTime(std::time_t t)
{
  using namespace std::chrono;
  const sys_seconds       tp{seconds{t}};
  const sys_days          date = floor<days>(tp);
  const year_month_day    ymd{date};
  const hh_mm_ss<seconds> tod{tp - date};

  std::tm out{};
  out.tm_year = static_cast<int>(ymd.year()) - 1900;
  out.tm_mon  = static_cast<int>(static_cast<unsigned>(ymd.month())) - 1;
  out.tm_mday = static_cast<int>(static_cast<unsigned>(ymd.day()));
  out.tm_hour = static_cast<int>(tod.hours().count());
  out.tm_min  = static_cast<int>(tod.minutes().count());
  out.tm_sec  = static_cast<int>(tod.seconds().count());
  return out;
}

}  // namespace miem
