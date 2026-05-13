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

option(MIEM_ENABLE_TESTS              "Build the test suite"              ON)
option(MIEM_ENABLE_MEMCHECK           "Enable memory checking in tests"   OFF)
option(MIEM_ENABLE_COVERAGE           "Enable code coverage output"       OFF)
option(MIEM_BUILD_DOCS                "Build the documentation"           OFF)
option(MIEM_USE_DOUBLE                "Use double-precision Real type"    ON)
option(MIEM_INSTALL                   "Generate install rules"            ON)

# Mass-conservation runtime check in FluxConverter: default ON in Debug,
# OFF in Release (per plan §D for `MIEM_CHECK_MASS_CONSERVATION`).
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

include(dependencies)

################################################################################
# MIEM library
#
# Three CMake targets per plan §D4:
#   miem_objects (OBJECT) — all core .cpp, shared between miem and miem_c
#   miem         (STATIC) — public C++ consumer face, alias musica::miem
#   miem_c       (STATIC) — C API; only built when MIEM_USE_DOUBLE=ON

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

if(MIEM_INSTALL AND PROJECT_IS_TOP_LEVEL)
  include(CMakePackageConfigHelpers)

  set(MIEM_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/miem")

  # Public targets — these are aliased into the `musica::` namespace by
  # the `install(EXPORT)` below and become the consumer-facing entry
  # points (`musica::miem`, optionally `musica::miem_c`).  `miem_objects`
  # is deliberately omitted: it is a build-system implementation detail
  # (its compiled .o files are baked into `miem` / `miem_c` via
  # `$<TARGET_OBJECTS:miem_objects>`) and consumers must not see it
  # surface as `musica::miem_objects`.
  set(MIEM_EXPORT_TARGETS miem)
  if(MIEM_USE_DOUBLE)
    list(APPEND MIEM_EXPORT_TARGETS miem_c)
  endif()

  install(TARGETS ${MIEM_EXPORT_TARGETS}
    EXPORT miemTargets
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY include/miem
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.h"
  )

  install(EXPORT miemTargets
    NAMESPACE musica::
    DESTINATION ${MIEM_INSTALL_CMAKEDIR}
    FILE miemTargets.cmake
  )

  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/miemConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/miemConfig.cmake"
    INSTALL_DESTINATION "${MIEM_INSTALL_CMAKEDIR}"
  )

  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/miemConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${PROJECT_BINARY_DIR}/miemConfig.cmake"
    "${PROJECT_BINARY_DIR}/miemConfigVersion.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/FindnetCDF.cmake"
    DESTINATION "${MIEM_INSTALL_CMAKEDIR}"
  )
endif()
