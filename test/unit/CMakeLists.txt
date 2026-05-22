add_subdirectory(version)

create_standard_test(NAME source SOURCES test_source.cpp)
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
