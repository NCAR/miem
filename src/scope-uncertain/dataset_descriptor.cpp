// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// NOT FOR v1 USE.  See ./README.md.

#include "dataset_descriptor.hpp"

// Intentionally empty.  `DatasetDescriptor::Default()` is defined inline in
// the header; the `FromYAML` function from `feature/scaffolding` has been
// deleted — schema parsing lives in MechanismConfiguration, not in MIEM.
