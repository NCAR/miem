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

# The UPTEMPO reader test reaches the internal reader (src/ on its include
# path) and reads both synthetic fixtures and the committed real subset
# (located via MIEM_TEST_DATA_DIR, set for every test by create_standard_test).
create_standard_test(
  NAME    uptempo_reader
  SOURCES test_uptempo_reader.cpp
  LIBRARIES miem_test_synthetic_nc
)
target_include_directories(test_uptempo_reader PRIVATE
  ${PROJECT_SOURCE_DIR}/src
)

create_standard_test(
  NAME    source_offline
  SOURCES test_source_offline.cpp
  LIBRARIES miem_test_synthetic_nc
)

create_standard_test(
  NAME    emissions
  SOURCES test_emissions.cpp
  LIBRARIES miem_test_synthetic_nc
)

create_standard_test(NAME emissions_builder SOURCES test_emissions_builder.cpp)
