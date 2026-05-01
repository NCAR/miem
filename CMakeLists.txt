cmake_minimum_required(VERSION 3.24)

# project and version must be on the same line so that the docs can extract it
project(miem VERSION 0.1.0
        LANGUAGES CXX
        DESCRIPTION "Model Independent Emissions Module"
        HOMEPAGE_URL "https://github.com/NCAR/miem")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

if(MIEM_ENABLE_COVERAGE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

include(GNUInstallDirs)

################################################################################
# Options

option(MIEM_ENABLE_TESTS    "Build the test suite"              ON)
option(MIEM_ENABLE_MEMCHECK "Enable memory checking in tests"   OFF)
option(MIEM_ENABLE_COVERAGE "Enable code coverage output"       OFF)
option(MIEM_BUILD_DOCS      "Build the documentation"           OFF)

set(MIEM_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})

################################################################################
# Dependencies

include(dependencies)

################################################################################
# MIEM library (header-only)

add_library(miem INTERFACE)
add_library(musica::miem ALIAS miem)

add_subdirectory(src)

target_compile_features(miem INTERFACE cxx_std_20)

target_include_directories(miem
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${MIEM_INSTALL_INCLUDE_DIR}>
)

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
