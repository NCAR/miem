# FindnetCDF.cmake
#
# Finds the NetCDF-C library using nc-config as a fallback when
# netCDFConfig.cmake is not available (common with Autotools/HPC installs).
#
# Sets:
#   netCDF_FOUND        - TRUE if NetCDF-C was found
#   netCDF_INCLUDE_DIRS - Include directories
#   netCDF_LIBRARIES    - Libraries to link
#   netCDF_VERSION      - Version string
#
# Creates imported target:
#   netCDF::netcdf      - The NetCDF-C library

if(netCDF_FOUND)
  return()
endif()

# Step 1: Try to find nc-config
find_program(NETCDF_NC_CONFIG nc-config
  HINTS ENV NETCDF_DIR ENV NETCDF_ROOT
  PATH_SUFFIXES bin
)

if(NETCDF_NC_CONFIG)
  # Query nc-config for installation details
  execute_process(
    COMMAND ${NETCDF_NC_CONFIG} --prefix
    OUTPUT_VARIABLE NETCDF_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${NETCDF_NC_CONFIG} --includedir
    OUTPUT_VARIABLE NETCDF_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${NETCDF_NC_CONFIG} --libdir
    OUTPUT_VARIABLE NETCDF_LIB_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${NETCDF_NC_CONFIG} --version
    OUTPUT_VARIABLE NETCDF_VERSION_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )

  # Parse version: "netCDF 4.9.3" -> "4.9.3"
  if(NETCDF_VERSION_RAW)
    string(REGEX REPLACE "^netCDF[ \t]+" "" netCDF_VERSION "${NETCDF_VERSION_RAW}")
  endif()

  # Find the actual library file
  find_library(netCDF_LIBRARY
    NAMES netcdf
    HINTS ${NETCDF_LIB_DIR} ${NETCDF_PREFIX}/lib ${NETCDF_PREFIX}/lib64
  )

  set(netCDF_INCLUDE_DIRS ${NETCDF_INCLUDE_DIR})
  set(netCDF_LIBRARIES ${netCDF_LIBRARY})

else()
  # Step 2: Fall back to manual search in standard paths
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
  set(netCDF_LIBRARIES ${netCDF_LIBRARY})
endif()

# Standard CMake find_package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(netCDF
  REQUIRED_VARS netCDF_LIBRARIES netCDF_INCLUDE_DIRS
  VERSION_VAR netCDF_VERSION
)

# Create imported target if found
if(netCDF_FOUND AND NOT TARGET netCDF::netcdf)
  add_library(netCDF::netcdf UNKNOWN IMPORTED)
  set_target_properties(netCDF::netcdf PROPERTIES
    IMPORTED_LOCATION "${netCDF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${netCDF_INCLUDE_DIRS}"
  )
endif()

mark_as_advanced(NETCDF_NC_CONFIG netCDF_LIBRARY NETCDF_INCLUDE_DIR)
