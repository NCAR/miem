add_subdirectory(version)
add_subdirectory(util)

create_standard_test(NAME miem_config SOURCES test_miem_config.cpp)
create_standard_test(NAME species_map SOURCES test_species_map.cpp)
