include(FetchContent)

# yaml-cpp for configuration parsing
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# GoogleTest for C++ unit testing
if(MIEM_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# NetCDF-C for emissions data I/O
find_package(netCDF REQUIRED)

# NetCDF-Fortran (only if building Fortran bindings)
if(MIEM_BUILD_FORTRAN)
  find_package(netCDF COMPONENTS Fortran QUIET)
endif()
