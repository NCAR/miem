################################################################################
# miem version header

configure_file(version.hpp.in
  ${PROJECT_SOURCE_DIR}/include/miem/version.hpp @ONLY)

################################################################################
# Core sources (no C API, no YAML, no MechanismConfiguration)

set(MIEM_CORE_SOURCES
  emissions_module.cpp
  emis_state.cpp
  source_offline.cpp
  source_factory.cpp
  species_map.cpp
  temporal_interpolator.cpp
  flux_converter.cpp
  eccad_reader.cpp
  config.cpp
  util/error.cpp
)

set(MIEM_C_API_SOURCES
  c_interface/miem_c_interface.cpp
  c_interface/emis_state_c_interface.cpp
)

################################################################################
# miem_objects: shared OBJECT library

add_library(miem_objects OBJECT ${MIEM_CORE_SOURCES})

target_include_directories(miem_objects
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(miem_objects PUBLIC cxx_std_20)
target_link_libraries(miem_objects PRIVATE netCDF::netcdf_normalized)

if(MIEM_USE_DOUBLE)
  target_compile_definitions(miem_objects PUBLIC MIEM_USE_DOUBLE)
endif()

if(MIEM_CHECK_MASS_CONSERVATION)
  target_compile_definitions(miem_objects PRIVATE MIEM_CHECK_MASS_CONSERVATION)
endif()

################################################################################
# miem: STATIC public C++ surface

add_library(miem STATIC)
target_link_libraries(miem
  PUBLIC  miem_objects
  PRIVATE netCDF::netcdf_normalized
)
add_library(musica::miem ALIAS miem)

################################################################################
# miem_c: STATIC C API.  Only built when MIEM_USE_DOUBLE=ON because the C API
# is unconditionally double-precision (static_assert in the .cpp).

if(MIEM_USE_DOUBLE)
  add_library(miem_c STATIC ${MIEM_C_API_SOURCES})
  target_include_directories(miem_c
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  target_compile_features(miem_c PUBLIC cxx_std_20)
  target_link_libraries(miem_c
    PUBLIC  miem_objects
    PRIVATE netCDF::netcdf_normalized
  )
  add_library(musica::miem_c ALIAS miem_c)
endif()
