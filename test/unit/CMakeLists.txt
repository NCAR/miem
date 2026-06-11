add_subdirectory(version)

create_standard_test(NAME source SOURCES test_source.cpp)
create_standard_test(NAME species_map SOURCES test_species_map.cpp)
create_standard_test(NAME temporal_interpolator SOURCES test_temporal_interpolator.cpp)
create_standard_test(NAME flux_converter SOURCES test_flux_converter.cpp)

# ECCADReader is internal: its header includes <netcdf.h>, so keeping it in
# src/internal/ (off the install path) keeps NetCDF out of MIEM's public
# headers and downstream consumers' include paths. The test reaches the
# reader by adding src/ to its include path (below) and links the synthetic
# NetCDF helper used to build fixtures.
create_standard_test(
  NAME    eccad_reader
  SOURCES test_eccad_reader.cpp
  LIBRARIES miem_test_synthetic_nc
)
target_include_directories(test_eccad_reader PRIVATE
  ${PROJECT_SOURCE_DIR}/src
)

create_standard_test(
  NAME    source_offline
  SOURCES test_source_offline.cpp
  LIBRARIES miem_test_synthetic_nc
)

# When udunits2 is FetchContent-built (no system install), point the tests that
# decode CF time units at its in-tree XML database. A system udunits2 supplies
# its own default path and needs no hint.
if(MIEM_UDUNITS2_XML_PATH)
  set_tests_properties(eccad_reader source_offline PROPERTIES
    ENVIRONMENT "UDUNITS2_XML_PATH=${MIEM_UDUNITS2_XML_PATH}")
endif()
