# MIEM

[Model Independent Emissions Module](https://github.com/NCAR/miem). MIEM can be used to configure and apply atmospheric emissions in host models.

[![Docker builds](https://github.com/NCAR/miem/actions/workflows/docker_and_coverage.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/docker_and_coverage.yml) [![Windows](https://github.com/NCAR/miem/actions/workflows/windows.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/windows.yml) [![Mac](https://github.com/NCAR/miem/actions/workflows/mac.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/mac.yml) [![Ubuntu](https://github.com/NCAR/miem/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/NCAR/miem/actions/workflows/ubuntu.yml)

Copyright (C) 2026 National Center for Atmospheric Research

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

The following example configures a single anthropogenic emissions source and runs one model timestep.

Species mapping and inventory translation are handled upstream by `MechanismConfiguration` and `musica::Translate()`. By the time `MIEMConfig` is constructed here, species are already resolved.

To run this example save the following code in a file named `miem_example.cpp`:

```cpp
#include <miem/config.hpp>
#include <miem/emissions_module.hpp>
#include <miem/emis_state.hpp>

#include <iostream>

using namespace miem;

int main()
{
  SourceConfig cams_anthro{
    .name_                   = "CAMS anthropogenic",
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

  MIEMConfig cfg{
    .version_ = "1.0.0",
    .sources_ = { cams_anthro },
  };

  EmissionsModule module(cfg, /*n_cells=*/163842, /*n_vert_levels=*/60);

  EmisState state = module.Run(
    86400.0 * 180.0,  // sim_time_sec: day 180
    600.0             // dt_sec: 10 minutes
  );

  std::cout << "NO surface flux at cell 0: "
            << state.surface_flux_(0, "NO")
            << " kg m-2 s-1" << std::endl;

  return 0;
}
```

To build and run the example (assuming the default install location):

```bash
g++ -o miem_example miem_example.cpp -I./include -std=c++20
./miem_example
```

Output:

```
NO surface flux at cell 0: 0 kg m-2 s-1
```

## Community and contributions

We welcome contributions and feedback from anyone, everything from updating the content or appearance of the documentation to new and cutting-edge science.

- **Collaboration**
  - Anyone interested in scientific collaboration which would add new software functionality should read the [MUSICA software development plan](https://github.com/NCAR/musica/blob/main/docs/Software%20Development%20Plan.pdf).

## Documentation

Please see the [MIEM documentation](https://miem.readthedocs.io/) for detailed installation and usage instructions.

## License

- [Apache 2.0](LICENSE)

Copyright (C) 2026 National Center for Atmospheric Research
