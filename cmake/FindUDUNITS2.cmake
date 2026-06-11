# Locate an installed UDUNITS-2 (header udunits2.h + libudunits2) and expose
# the imported target UDUNITS2::udunits2. Like FindnetCDF.cmake, this covers
# system/HPC/package-manager installs (libudunits2-dev, Homebrew, Conda) that
# ship no CMake config package; dependencies.cmake FetchContent-builds it when
# this search fails.

find_path(UDUNITS2_INCLUDE_DIR
  NAMES udunits2.h
  PATH_SUFFIXES udunits2)

find_library(UDUNITS2_LIBRARY
  NAMES udunits2 udunits)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UDUNITS2
  REQUIRED_VARS UDUNITS2_LIBRARY UDUNITS2_INCLUDE_DIR)

if(UDUNITS2_FOUND AND NOT TARGET UDUNITS2::udunits2)
  add_library(UDUNITS2::udunits2 UNKNOWN IMPORTED)
  set_target_properties(UDUNITS2::udunits2 PROPERTIES
    IMPORTED_LOCATION "${UDUNITS2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${UDUNITS2_INCLUDE_DIR}")
endif()

mark_as_advanced(UDUNITS2_INCLUDE_DIR UDUNITS2_LIBRARY)
