set(MIEM_CORE_SOURCES
  emissions_module.cpp
  emis_state.cpp
  source_offline.cpp
  source_factory.cpp
  species_map.cpp
  temporal_interpolator.cpp
  flux_converter.cpp
  config.cpp
  dataset_descriptor.cpp
  ses_reader.cpp
  util/error.cpp
)

set(MIEM_C_API_SOURCES
  c_interface/miem_c_interface.cpp
  c_interface/emis_state_c_interface.cpp
)

# C API requires double precision (static_assert enforced)
if(MIEM_DOUBLE_PRECISION)
  add_library(miem ${MIEM_CORE_SOURCES} ${MIEM_C_API_SOURCES})
else()
  add_library(miem ${MIEM_CORE_SOURCES})
endif()

add_library(musica::miem ALIAS miem)

target_include_directories(miem
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(miem
  PRIVATE
    yaml-cpp::yaml-cpp
    netCDF::netcdf
)
