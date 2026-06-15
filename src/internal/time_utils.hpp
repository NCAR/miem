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

// UTC calendar fields -> a system-clock time point (second resolution).
// Fields use the natural calendar convention (full year, 1-based month and
// day), so callers assembling a time from parsed digits can skip the std::tm
// intermediate entirely.
inline std::chrono::sys_seconds UtcTimePoint(int year, int month, int day,
                                             int hour, int minute, int second)
{
  using namespace std::chrono;
  const year_month_day ymd{std::chrono::year{year} /
                           std::chrono::month{static_cast<unsigned>(month)} /
                           std::chrono::day{static_cast<unsigned>(day)}};
  return sys_days{ymd} + hours{hour} + minutes{minute} + seconds{second};
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
