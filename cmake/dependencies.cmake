include(FetchContent)

################################################################################
# NetCDF-C — required.
#
# Try `find_package(netCDF)` (Conda/Homebrew/system installs ship one of two
# config-package layouts).  Fall back to the in-tree FindnetCDF.cmake which
# queries `nc-config` for Autotools/HPC builds, and finally FetchContent to a
# pinned tag if nothing is installed.
#
# We expose a single normalized target `netCDF::netcdf_normalized` for the
# rest of the build to consume, so consumers do not have to disambiguate
# the casing (`netCDF::netCDF` vs `netCDF::netcdf`) across providers.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# Honor the $NETCDF env variable set by HPC module systems (e.g. Derecho).
if(DEFINED ENV{NETCDF} AND NOT "$ENV{NETCDF}" STREQUAL "")
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{NETCDF}")
endif()

# Try a system/module-installed netCDF first; fall back to FetchContent.
# Note: FIND_PACKAGE_ARGS in FetchContent_Declare passes extra args to
# find_package using the *content name* as the package name — it cannot
# override the name, so FetchContent_Declare(netcdf-c ... FIND_PACKAGE_ARGS netCDF)
# would call find_package(netcdf-c netCDF), which is invalid.
# We therefore call find_package explicitly before FetchContent.
find_package(netCDF QUIET)

if(NOT netCDF_FOUND)
  set(BUILD_UTILITIES OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTS    OFF CACHE BOOL "" FORCE)
  set(ENABLE_NETCDF_4 ON  CACHE BOOL "" FORCE)
  FetchContent_Declare(netcdf-c
    GIT_REPOSITORY https://github.com/Unidata/netcdf-c.git
    GIT_TAG v4.9.2
  )
  FetchContent_MakeAvailable(netcdf-c)
endif()

if(NOT TARGET netCDF::netcdf_normalized)
  add_library(netCDF::netcdf_normalized INTERFACE IMPORTED)
  if(TARGET netCDF::netCDF)
    target_link_libraries(netCDF::netcdf_normalized INTERFACE netCDF::netCDF)
  elseif(TARGET netCDF::netcdf)
    target_link_libraries(netCDF::netcdf_normalized INTERFACE netCDF::netcdf)
  elseif(TARGET netcdf)
    target_link_libraries(netCDF::netcdf_normalized INTERFACE netcdf)
  else()
    message(FATAL_ERROR
      "NetCDF was reported as found but no known imported target exists "
      "(tried netCDF::netCDF, netCDF::netcdf, netcdf).")
  endif()
endif()

################################################################################
# Google Test (only when MIEM_ENABLE_TESTS=ON).

if(MIEM_ENABLE_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
  )

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  set_target_properties(gtest      PROPERTIES CXX_CLANG_TIDY "")
  set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")
endif()

################################################################################
# Docs

if(MIEM_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  find_package(Sphinx REQUIRED)
endif()
