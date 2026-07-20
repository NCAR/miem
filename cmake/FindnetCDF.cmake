# FindnetCDF.cmake
#
# Fallback NetCDF-C finder for installations that do not ship a CMake
# config package (Autotools/HPC/MSYS2).  Tries pkg-config first, then
# `nc-config`, then falls back to manual search of standard prefixes.
#
# Variables set on success:
#   netCDF_FOUND        - TRUE if NetCDF-C was found
#   netCDF_INCLUDE_DIRS - Include directories
#   netCDF_LIBRARIES    - Libraries to link
#   netCDF_VERSION      - Version string
#
# Imported target created on success:
#   netCDF::netcdf      - The NetCDF-C library

if(netCDF_FOUND)
  return()
endif()

# Try pkg-config first. `nc-config` (tried below) is a POSIX shell script,
# and on Windows/MSYS2 the `cmake` binary is a native Win32 program: its
# execute_process() goes through plain CreateProcess, which can't interpret
# a shebang line, so nc-config silently produces empty output there even
# when found. pkg-config is a real compiled binary everywhere it exists
# (MSYS2 included), so it doesn't have that problem.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(NETCDF_PC QUIET IMPORTED_TARGET netcdf)
endif()

if(NETCDF_PC_FOUND)
  set(netCDF_INCLUDE_DIRS ${NETCDF_PC_INCLUDE_DIRS})
  set(netCDF_LIBRARIES    ${NETCDF_PC_LINK_LIBRARIES})
  set(netCDF_VERSION      ${NETCDF_PC_VERSION})

  if(NOT TARGET netCDF::netcdf)
    add_library(netCDF::netcdf ALIAS PkgConfig::NETCDF_PC)
  endif()

else()
  # Try nc-config next.
  find_program(NETCDF_NC_CONFIG nc-config
    HINTS ENV NETCDF_DIR ENV NETCDF_ROOT
    PATH_SUFFIXES bin
  )

  if(NETCDF_NC_CONFIG)
    execute_process(COMMAND ${NETCDF_NC_CONFIG} --prefix
      OUTPUT_VARIABLE NETCDF_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${NETCDF_NC_CONFIG} --includedir
      OUTPUT_VARIABLE NETCDF_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${NETCDF_NC_CONFIG} --libdir
      OUTPUT_VARIABLE NETCDF_LIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${NETCDF_NC_CONFIG} --version
      OUTPUT_VARIABLE NETCDF_VERSION_RAW OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

    if(NETCDF_VERSION_RAW)
      string(REGEX REPLACE "^netCDF[ \t]+" "" netCDF_VERSION "${NETCDF_VERSION_RAW}")
    endif()

    find_library(netCDF_LIBRARY
      NAMES netcdf
      HINTS ${NETCDF_LIB_DIR} ${NETCDF_PREFIX}/lib ${NETCDF_PREFIX}/lib64
    )

    set(netCDF_INCLUDE_DIRS ${NETCDF_INCLUDE_DIR})
    set(netCDF_LIBRARIES    ${netCDF_LIBRARY})
  endif()

  # Either nc-config wasn't found, or (as on native-Windows CMake) it was
  # found but couldn't actually be executed, leaving the variables above
  # empty. Fall back to a direct filesystem search either way -- this also
  # picks up the NETCDF_ROOT/NETCDF_DIR hint directly instead of relying on
  # nc-config's (possibly broken) output.
  if(NOT netCDF_INCLUDE_DIRS OR NOT netCDF_LIBRARIES)
    find_path(NETCDF_INCLUDE_DIR
      NAMES netcdf.h
      HINTS ENV NETCDF_DIR ENV NETCDF_ROOT
      PATH_SUFFIXES include
    )
    find_library(netCDF_LIBRARY
      NAMES netcdf
      HINTS ENV NETCDF_DIR ENV NETCDF_ROOT
      PATH_SUFFIXES lib lib64
    )

    set(netCDF_INCLUDE_DIRS ${NETCDF_INCLUDE_DIR})
    set(netCDF_LIBRARIES    ${netCDF_LIBRARY})
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(netCDF
  REQUIRED_VARS netCDF_LIBRARIES netCDF_INCLUDE_DIRS
  VERSION_VAR netCDF_VERSION
)

if(netCDF_FOUND AND NOT TARGET netCDF::netcdf)
  add_library(netCDF::netcdf UNKNOWN IMPORTED)
  set_target_properties(netCDF::netcdf PROPERTIES
    IMPORTED_LOCATION "${netCDF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${netCDF_INCLUDE_DIRS}"
  )
endif()

mark_as_advanced(NETCDF_NC_CONFIG netCDF_LIBRARY NETCDF_INCLUDE_DIR)
