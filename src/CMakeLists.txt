################################################################################
# miem version header

configure_file(version.hpp.in
  ${PROJECT_SOURCE_DIR}/include/miem/version.hpp @ONLY)

################################################################################
# Foundation sources (PR 1 slice).
#
# Subsequent PRs add domain sources (algorithms, config, data layer,
# runtime).  All of those will land in `MIEM_CORE_SOURCES` here.

set(MIEM_CORE_SOURCES
  util/error.cpp
)

################################################################################
# miem_objects: shared OBJECT library

add_library(miem_objects OBJECT ${MIEM_CORE_SOURCES})

target_include_directories(miem_objects
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    # `internal/` headers (e.g., ECCADReader) live under src/ and must
    # not be installed -- include them PRIVATEly so consumers cannot
    # accidentally depend on them.
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(miem_objects PUBLIC cxx_std_20)

if(MIEM_USE_DOUBLE)
  target_compile_definitions(miem_objects PUBLIC MIEM_USE_DOUBLE)
endif()

################################################################################
# miem: STATIC public C++ surface

add_library(miem STATIC)
target_sources(miem PRIVATE $<TARGET_OBJECTS:miem_objects>)
target_include_directories(miem
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(miem PUBLIC cxx_std_20)
if(MIEM_USE_DOUBLE)
  target_compile_definitions(miem PUBLIC MIEM_USE_DOUBLE)
endif()
add_library(musica::miem ALIAS miem)
