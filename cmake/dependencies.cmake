include(FetchContent)

################################################################################
# NetCDF-C — required.
#
# Try `find_package(netCDF)` (Conda/Homebrew/system installs ship one of two
# config-package layouts).  Fall back to the in-tree FindnetCDF.cmake which
# queries `nc-config` for Autotools/HPC builds.  MIEM does not build netCDF
# itself, because a netCDF-4 build also needs HDF5.
#
# We expose a single normalized target `netCDF::netcdf_normalized` for the
# rest of the build to consume, so consumers do not have to disambiguate
# the casing (`netCDF::netCDF` vs `netCDF::netcdf`) across providers.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# Honor the $NETCDF env variable set by HPC module systems (e.g. Derecho).
if(DEFINED ENV{NETCDF} AND NOT "$ENV{NETCDF}" STREQUAL "")
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{NETCDF}")
endif()

find_package(netCDF QUIET)

if(NOT netCDF_FOUND)
  message(FATAL_ERROR
    "MIEM needs netCDF-C, but CMake cannot find it. Load a netCDF module, put nc-config "
    "on PATH, set NETCDF to the netCDF install prefix, or add the prefix to CMAKE_PREFIX_PATH.")
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
