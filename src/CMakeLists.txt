################################################################################
# miem version header

configure_file(version.hpp.in
  ${PROJECT_SOURCE_DIR}/include/miem/version.hpp @ONLY)

################################################################################
# miem library

if(MIEM_BUILD_SHARED_LIBS)
  add_library(miem SHARED)
else()
  add_library(miem STATIC)
endif()

target_sources(miem PRIVATE
  emissions.cpp
  emissions_builder.cpp
  emissions_state.cpp
  source_offline.cpp
  source_factory.cpp
  species_map.cpp
  temporal_interpolator.cpp
  flux_converter.cpp
  eccad_reader.cpp
  uptempo_reader.cpp
  reader_factory.cpp
)

target_link_libraries(miem PRIVATE netCDF::netcdf_normalized)

target_include_directories(miem
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(miem PUBLIC cxx_std_20)

if(MIEM_USE_DOUBLE)
  target_compile_definitions(miem PUBLIC MIEM_USE_DOUBLE)
endif()

if(MIEM_CHECK_MASS_CONSERVATION)
  target_compile_definitions(miem PRIVATE MIEM_CHECK_MASS_CONSERVATION)
endif()

add_library(musica::miem ALIAS miem)
