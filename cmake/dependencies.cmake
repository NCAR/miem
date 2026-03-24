include(FetchContent)

# yaml-cpp for configuration parsing
# Try system install first, fetch if not found
find_package(yaml-cpp 0.7 QUIET)
if(NOT yaml-cpp_FOUND)
  FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
  )
  set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(yaml-cpp)
  set(MIEM_FETCHED_YAML_CPP TRUE CACHE INTERNAL "")
else()
  set(MIEM_FETCHED_YAML_CPP FALSE CACHE INTERNAL "")
endif()

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
# First tries netCDFConfig.cmake (standard installs), then falls back to
# cmake/FindnetCDF.cmake which queries nc-config (Autotools/HPC installs).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(netCDF REQUIRED)

# Normalize target name: config-mode creates netCDF::netCDF (capital C),
# our Find module creates netCDF::netcdf (lowercase). Alias to ensure
# a consistent target name for src/CMakeLists.txt.
if(TARGET netCDF::netCDF AND NOT TARGET netCDF::netcdf)
  add_library(netCDF::netcdf ALIAS netCDF::netCDF)
endif()
