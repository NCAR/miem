// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>

namespace miem {

// Real-precision typedef.  Default ON via the MIEM_USE_DOUBLE cmake option.
// The C API (src/c_interface/) static_asserts that Real == double internally
// so it can hand back raw `double*` buffers without copy.
#ifdef MIEM_USE_DOUBLE
using Real = double;
#else
using Real = float;
#endif

using Index = std::size_t;

}  // namespace miem
