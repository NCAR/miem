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

################################################################################
# Dependencies
#
# PR 1 (build foundation) sets up `find_package(netCDF)` so the search
# logic, FetchContent fallback, and `netCDF::netcdf_normalized` alias are
# in place — but no library target links against it yet.  Consumers come
# online in PR 4 (data layer).

include(dependencies)

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
