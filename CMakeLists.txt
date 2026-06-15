cmake_minimum_required(VERSION 3.24)

# project and version must be on the same line so that the docs can extract it
project(miem VERSION 0.1.0
        LANGUAGES CXX
        DESCRIPTION "Model Independent Emissions Module"
        HOMEPAGE_URL "https://github.com/NCAR/miem")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

################################################################################
# Options

option(MIEM_ENABLE_TESTS              "Build the test suite"              OFF)
option(MIEM_ENABLE_MEMCHECK           "Enable memory checking in tests"   OFF)
option(MIEM_ENABLE_COVERAGE           "Enable code coverage output"       OFF)
option(MIEM_BUILD_DOCS                "Build the documentation"           OFF)
option(MIEM_USE_DOUBLE                "Use double-precision Real type"    ON)
option(MIEM_BUILD_SHARED_LIBS         "Build shared libraries"            OFF)

# Mass-conservation runtime check in FluxConverter: default ON in Debug,
# OFF in Release.
if(NOT DEFINED MIEM_CHECK_MASS_CONSERVATION)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MIEM_CHECK_MASS_CONSERVATION ON CACHE BOOL
        "Runtime mass-conservation check in FluxConverter (debug default ON)")
  else()
    set(MIEM_CHECK_MASS_CONSERVATION OFF CACHE BOOL
        "Runtime mass-conservation check in FluxConverter (release default OFF)")
  endif()
endif()

################################################################################
# Dependencies
#
# PR 1 (build foundation) sets up `find_package(netCDF)` so the search
# logic, FetchContent fallback, and `netCDF::netcdf_normalized` alias are
# in place — but no library target links against it yet.  Consumers come
# online in PR 4 (data layer).

include(dependencies)

################################################################################
# Code coverage instrumentation
#
# Apply the coverage compiler flags BEFORE the library and test targets are
# defined, so both the compiled library (src/) and the tests are instrumented
# and emit .gcno files.  (micm calls append_coverage_compiler_flags() the same
# way, but micm is header-only; MIEM has a compiled library, so the flags must
# precede add_subdirectory(src).)  The `coverage` make-target itself is defined
# after the tests, below.

if(MIEM_ENABLE_TESTS AND MIEM_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CodeCoverage)
    append_coverage_compiler_flags()
  else()
    message(FATAL_ERROR "Coverage is only supported with GCC or Clang")
  endif()
endif()

################################################################################
# MIEM library (foundation slice)

add_subdirectory(src)

################################################################################
# Tests

if(MIEM_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(test)

  if(MIEM_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      include(CodeCoverage)
      setup_target_for_coverage_lcov(
          NAME coverage
          EXECUTABLE ctest
          EXCLUDE "${PROJECT_SOURCE_DIR}/test/*"
          BASE_DIRECTORY "${PROJECT_SOURCE_DIR}/include"
          LCOV_ARGS --ignore-errors mismatch --ignore-errors unused
      )
    else()
      message(FATAL_ERROR "Coverage is only supported with GCC or Clang")
    endif()
  endif()
endif()

################################################################################
# Docs

if(MIEM_BUILD_DOCS)
  add_subdirectory(docs)
endif()

################################################################################
# Install / config-package

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(packaging)
endif()
