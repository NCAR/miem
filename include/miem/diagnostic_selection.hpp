// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Explicit bounded selection of disaggregated emissions diagnostics.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace miem
{

  struct DiagnosticSelection
  {
    std::vector<std::string> sectors_;
    std::vector<int> categories_;
    bool layered_output_ = false;
    std::size_t max_fields_ = 0;

    bool Empty() const
    {
      return sectors_.empty() && categories_.empty();
    }
  };

}  // namespace miem
