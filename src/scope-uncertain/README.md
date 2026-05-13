# `src/scope-uncertain/`

These files exist to land scaffolding code that may belong in v1 but is
**not in scope for this port**.  They are not compiled into MIEM in v1
and are not on the install path.  Each subdirectory README explains the
gate (config field, runtime check) that determines whether the code is
dead in v1.

## `dataset_descriptor.{hpp,cpp}`

`DatasetDescriptor` adapts non-ECCAD NetCDF files (legacy CEDS, custom
inventories) to the canonical ECCAD layout.  In v1 the only accepted
`Inventory.convention` is `"eccad"` — `MIEMConfig::Validate()` rejects
anything else with `UnknownConvention` before any descriptor-touching
code can run.

When v1.1 lands `convention: descriptor`, these files move back to
`include/miem/dataset_descriptor.hpp` and `src/dataset_descriptor.cpp`,
the `OfflineEmissionSource` descriptor branch is reactivated, and the
`MIEMConfig::Validate()` rejection becomes an accept.  The struct
definition is kept here verbatim so the resurrection is mechanical.

`FromYAML` is deliberately absent — schema parsing now lives in
`MechanismConfiguration` and is not part of MIEM at any level.
