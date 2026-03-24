cmake_minimum_required(VERSION 3.14)
project(MIEM
  VERSION 1.0.0
  LANGUAGES C CXX
  DESCRIPTION "Model Independent Emissions Module"
)

option(MIEM_BUILD_FORTRAN "Build Fortran bindings" ON)
option(MIEM_BUILD_TESTS "Build test suite" ON)
option(MIEM_DOUBLE_PRECISION "Use 64-bit floats" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MIEM_DOUBLE_PRECISION)
  add_compile_definitions(MIEM_USE_DOUBLE)
endif()

include(cmake/dependencies.cmake)

add_subdirectory(src)

if(MIEM_BUILD_FORTRAN)
  enable_language(Fortran)
  add_subdirectory(fortran)
endif()

if(MIEM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Install rules
set(MIEM_INSTALL_TARGETS miem)

# If we fetched yaml-cpp via FetchContent, it must be in the export set
# because CMake traces static library PRIVATE deps during install(EXPORT).
if(MIEM_FETCHED_YAML_CPP)
  list(APPEND MIEM_INSTALL_TARGETS yaml-cpp)
endif()

install(TARGETS ${MIEM_INSTALL_TARGETS}
  EXPORT MIEMTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)

install(DIRECTORY include/miem/ DESTINATION include/miem)

# Generate and install CMake package config
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(EXPORT MIEMTargets
  NAMESPACE musica::
  DESTINATION lib/cmake/MIEM
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MIEMConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfig.cmake"
  @ONLY
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfigVersion.cmake"
  DESTINATION lib/cmake/MIEM
)

# Install FindnetCDF.cmake so downstream consumers can re-find netCDF
install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindnetCDF.cmake"
  DESTINATION lib/cmake/MIEM
)
