==========
Quickstart
==========

This example shows how to configure and run one emissions timestep. It
mirrors how a host model (e.g. MPAS-A) drives MIEM directly in C++, without
going through the YAML parsing layer.

Species mapping and inventory translation are handled upstream by
``MechanismConfiguration`` and ``musica::Translate()``. By the time a
``Source`` reaches MIEM, species are already resolved.

Describing a Source
===================

A ``Source`` is the description of one emission inventory -- the emissions
analog of a single ``micm::Process``.

.. code-block:: cpp

   #include <miem/miem.hpp>

   using namespace miem;

   Source cams_anthro{
     .name_                   = "cams anthro",
     .mode_                   = SourceMode::Offline,
     .type_                   = SourceType::Anthropogenic,
     .file_pattern_           = "/path/to/CAMS-GLOB-ANT_{YYYY}-{MM}.nc",
     .temporal_interpolation_ = TemporalInterpolation::Linear,
     .vertical_injection_     = VerticalInjection::Surface,
     .category_               = 0,
     .hierarchy_              = 1,
     .scaling_factor_         = 1.0,
     .sector_                 = "anthropogenic",
   };

Building the Module and Running
===============================

Following micm's ``CpuSolverBuilder``, MIEM has no aggregate config object:
``EmissionsBuilder`` takes the ``Source`` domain objects and validates and
builds the runtime module in ``Build()`` (which throws
``miem::MiemException`` if the configuration is invalid).

.. code-block:: cpp

   Emissions module = EmissionsBuilder()
                          .SetGridDimensions(/*n_cells=*/163842,
                                             /*n_vert_levels=*/60)
                          .AddSource(cams_anthro)
                          .Build();

   EmissionsState state = module.Run(
     86400.0 * 180.0,  // sim_time_sec: day 180
     600.0             // dt_sec: 10 minutes
   );

Reading the Output
==================

.. code-block:: cpp

   double no_flux = state.surface_flux_(0, "NO");  // kg m-2 s-1 at cell 0

   double anthro_no = state.sector_fluxes_.at("anthropogenic")(0, "NO");

See ``examples/full_example.cpp`` for the complete worked example including
multiple layered sources and sector diagnostics.
