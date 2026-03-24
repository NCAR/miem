include(GoogleTest)

# C++ unit tests
add_executable(test_emis_state unit/test_emis_state.cpp)
target_link_libraries(test_emis_state PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_emis_state)

add_executable(test_flux_converter unit/test_flux_converter.cpp)
target_link_libraries(test_flux_converter PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_flux_converter)

add_executable(test_species_map unit/test_species_map.cpp)
target_link_libraries(test_species_map PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_species_map)

add_executable(test_temporal_interpolator unit/test_temporal_interpolator.cpp)
target_link_libraries(test_temporal_interpolator PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_temporal_interpolator)

add_executable(test_config unit/test_config.cpp)
target_link_libraries(test_config PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_config)

add_executable(test_ces_reader unit/test_ces_reader.cpp)
target_link_libraries(test_ces_reader PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_ces_reader)

add_executable(test_emissions_module unit/test_emissions_module.cpp)
target_link_libraries(test_emissions_module PRIVATE miem GTest::gtest_main)
gtest_discover_tests(test_emissions_module)

# Copy test data to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# Fortran tests
if(MIEM_BUILD_FORTRAN)
  add_executable(test_miem_fortran fortran/test_miem_fortran.F90)
  target_link_libraries(test_miem_fortran PRIVATE miem-fortran)
  add_test(NAME test_miem_fortran COMMAND test_miem_fortran)
endif()
