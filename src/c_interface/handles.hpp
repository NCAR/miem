// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Internal-only handle definitions for the MIEM C API.  The opaque
// `miem_config_t`, `miem_t`, and `miem_state_t` structs are forward-
// declared in the public header `miem/miem_c.h`; this header carries
// their concrete layout so that *every* C-API translation unit sees an
// identical definition (ODR).  Including this from multiple TUs is the
// only sanctioned way to widen the implementation.
//
// This file is NOT installed.  Downstream consumers must continue to
// see the structs as opaque.
#pragma once

#include <memory>

#include "miem/config.hpp"
#include "miem/emissions_state.hpp"
#include "miem/emissions.hpp"

struct miem_config_t
{
  ::miem::MIEMConfig cfg_;
};

struct miem_t
{
  std::unique_ptr<::miem::Emissions> module_;
};

struct miem_state_t
{
  ::miem::EmissionsState state_;
};
