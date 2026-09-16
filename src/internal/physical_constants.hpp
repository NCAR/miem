// Copyright (C) 2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace miem
{

  // CODATA 2018 / SI-exact value, matching the constant used to derive the
  // molecular-weight-based scaling factors already hardcoded elsewhere in
  // this project's emissions configs (e.g. the FINN species map).
  inline constexpr double kAvogadroNumber = 6.02214076e23;

}  // namespace miem
