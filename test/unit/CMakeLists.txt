add_subdirectory(version)
add_subdirectory(util)

create_standard_test(NAME emissions_config SOURCES test_emissions_config.cpp)
create_standard_test(NAME species_map SOURCES test_species_map.cpp)
create_standard_test(NAME temporal_interpolator SOURCES test_temporal_interpolator.cpp)
create_standard_test(NAME flux_converter SOURCES test_flux_converter.cpp)

# ECCADReader lives in src/internal/ (kept off the install path because
# its NetCDF surface throws).  The test links the synthetic NC helper
# and the internal header lookup is wired via target_include_directories
# below — there is no public-API way to reach the reader.
create_standard_test(
  NAME    eccad_reader
  SOURCES test_eccad_reader.cpp
  LIBRARIES miem_test_synthetic_nc
)
target_include_directories(test_eccad_reader PRIVATE
  ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(test_eccad_reader PRIVATE netCDF::netcdf_normalized)

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

# Build/install meta-tests.  These scan the source tree (not the
# installed tree) and assert the export-set invariants documented in
# the plan (M1) and the "no throwing public headers" criterion #12.
create_standard_test(
  NAME    install_export
  SOURCES test_install_export.cpp
)
target_compile_definitions(test_install_export PRIVATE
  MIEM_TEST_PROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"
)
