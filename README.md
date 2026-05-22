# MIEM

Model Independent Emissions Module. MIEM can be used to configure and apply atmospheric emissions in host models.

[![GitHub Releases](https://img.shields.io/github/release/NCAR/miem.svg)](https://github.com/NCAR/miem/releases) [![Docker builds](https://github.com/NCAR/miem/actions/workflows/docker_and_coverage.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/docker_and_coverage.yml) [![Ubuntu](https://github.com/NCAR/miem/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/ubuntu.yml) [![Mac](https://github.com/NCAR/miem/actions/workflows/mac.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/mac.yml) [![Windows](https://github.com/NCAR/miem/actions/workflows/windows.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/windows.yml) [![Clang Tidy](https://github.com/NCAR/miem/actions/workflows/clang-tidy.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/clang-tidy.yml) [![Clang Format](https://github.com/NCAR/miem/actions/workflows/clang-format.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/clang-format.yml)

Copyright (C) 2026 University Corporation for Atmospheric Research

## Getting Started

### Installing MIEM locally

To build and install MIEM locally, you must have CMake installed on your machine.

Open a terminal window, navigate to a folder where you would like the MIEM files to exist, and run the following commands:

```bash
git clone https://github.com/NCAR/miem.git
cd miem
mkdir build
cd build
cmake ..
sudo make install
```

To run the tests:

```bash
make test
```

If you would later like to uninstall MIEM, you can run `sudo make uninstall` from the `build/` directory.

### Running a MIEM Docker container

You must have [Docker Desktop](https://www.docker.com/get-started) installed and running. With Docker Desktop running, open a terminal window and run:

```bash
git clone https://github.com/NCAR/miem.git
cd miem
docker build -t miem -f docker/Dockerfile .
docker run -it miem bash
```

Inside the container, you can run the MIEM tests from the `/build/` folder:

```bash
cd /build/
make test
```

## Using the MIEM API

The following example describes a single anthropogenic emissions source and assembles it into an `Emissions` module with `EmissionsBuilder`.  Following micm, MIEM has no aggregate configuration object: you hand the builder `Source` domain objects and it validates and builds the module in `Build()`.

Species mapping and inventory translation are handled upstream by `MechanismConfiguration` and `musica::Translate()`.  By the time a `Source` is constructed here, every named reference has been resolved into a flat struct that MIEM owns.  MIEM does not parse YAML at any level.

To run this example save the following code in a file named `miem_example.cpp`:

```cpp
#include <miem/miem.hpp>

#include <iostream>

using namespace miem;

int main()
{
  Source cams_anthro;
  cams_anthro.name_                   = "CAMS anthropogenic";
  cams_anthro.mode_                   = SourceMode::Offline;
  cams_anthro.type_                   = SourceType::Anthropogenic;
  cams_anthro.file_pattern_           = "/path/to/CAMS-GLOB-ANT_{YYYY}-{MM}.nc";
  cams_anthro.convention_             = "eccad";
  cams_anthro.temporal_interpolation_ = TemporalInterpolation::Linear;
  cams_anthro.vertical_injection_     = VerticalInjection::Surface;
  cams_anthro.category_               = 0;
  cams_anthro.hierarchy_              = 1;
  cams_anthro.scaling_factor_         = 1.0;
  cams_anthro.sector_                 = "anthropogenic";

  // Programmatic species map: NOx -> NO (0.9), NOx -> NO2 (0.1).
  cams_anthro.species_map_.AddMapping("NOx", "NO",  0.9);
  cams_anthro.species_map_.AddMapping("NOx", "NO2", 0.1);

  try
  {
    // EmissionsBuilder assembles Source descriptions into a runtime
    // module, mirroring micm's CpuSolverBuilder.  Build() validates the
    // configuration and throws miem::MiemException if any invariant fails.
    Emissions emissions = EmissionsBuilder()
                              .SetGridDimensions(/*n_cells=*/163842,
                                                 /*n_vert_levels=*/60)
                              .AddSource(cams_anthro)
                              .Build();

    std::cout << "emissions advertises " << emissions.NumSpecies()
              << " mechanism species\n";

    // emissions.Run(sim_time_sec, dt_sec) returns an EmissionsState and
    // throws miem::MiemException on failure; the overload taking
    // air_density/layer_thickness arrays additionally populates
    // state.tendency_ via FluxConverter.
  }
  catch (const miem::MiemException& e)
  {
    std::cerr << "configuration invalid [" << e.Category() << '/' << e.Code()
              << "]: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
```

To build and run the example with the installed `find_package(miem)` config:

```cmake
find_package(miem CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE musica::miem)
```

## Community and contributions

We welcome contributions and feedback from anyone, everything from updating the content or appearance of the documentation to new and cutting-edge science.

- **Collaboration**
  - Anyone interested in scientific collaboration which would add new software functionality should read the [MUSICA software development plan](https://github.com/NCAR/musica/blob/main/docs/Software%20Development%20Plan.pdf).

## Documentation

Please see the [MIEM documentation](https://miem.readthedocs.io/) for detailed installation and usage instructions.

## License

- [Apache 2.0](LICENSE)
