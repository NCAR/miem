// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// MIEM error categories (strings) and codes (integers), defined as
// `#define` macros (modeled on `micm/util/error.hpp`) so the same
// definitions can be shared by C++ and (eventually) the Fortran interface
// without duplication.  Failures are reported by throwing
// `miem::MiemException` (see `miem_exception.hpp`) with one of these
// (category, code) pairs; `musica::HandleErrors()` catches it at the
// C boundary and converts it to a MUSICA `Error` struct, preserving both.
// Codes restart at 1 within each category, matching the MICM convention.
#pragma once

// --- Configuration ---------------------------------------------------
#define MIEM_ERROR_CATEGORY_CONFIGURATION                       "MIEM Configuration"
#define MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_REGRIDDING         1
#define MIEM_CONFIGURATION_ERROR_CODE_UNKNOWN_CONVENTION             2
#define MIEM_CONFIGURATION_ERROR_CODE_ONLINE_NOT_SUPPORTED           3
#define MIEM_CONFIGURATION_ERROR_CODE_UNSUPPORTED_VERTICAL_INJECTION 4
#define MIEM_CONFIGURATION_ERROR_CODE_DUPLICATE_CATEGORY_HIERARCHY   5

// --- Species ---------------------------------------------------------
#define MIEM_ERROR_CATEGORY_SPECIES                    "MIEM Species"
#define MIEM_SPECIES_ERROR_CODE_SCALING_EXCEEDS_ONE    1

// --- Validation ------------------------------------------------------
#define MIEM_ERROR_CATEGORY_VALIDATION                 "MIEM Validation"
#define MIEM_VALIDATION_ERROR_CODE_CELL_COUNT_MISMATCH 1
#define MIEM_VALIDATION_ERROR_CODE_MASS_CONSERVATION   2
