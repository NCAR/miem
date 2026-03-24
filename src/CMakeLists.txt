add_library(miem
  emissions_module.cpp
  emis_state.cpp
  source_offline.cpp
  species_map.cpp
  temporal_interpolator.cpp
  flux_converter.cpp
  config.cpp
  dataset_descriptor.cpp
  ces_reader.cpp
  util/error.cpp
  c_interface/miem_c_interface.cpp
  c_interface/emis_state_c_interface.cpp
)

add_library(musica::miem ALIAS miem)

target_include_directories(miem
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(miem
  PUBLIC
    yaml-cpp::yaml-cpp
    netCDF::netcdf
)
