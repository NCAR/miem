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
  util/error.cpp
)

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

add_library(musica::miem ALIAS miem)
