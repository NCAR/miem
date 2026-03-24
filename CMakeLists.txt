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

include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/MIEMConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfig.cmake"
  INSTALL_DESTINATION lib/cmake/MIEM
)

install(TARGETS miem
  EXPORT MIEMTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)

install(DIRECTORY include/miem/ DESTINATION include/miem)

install(EXPORT MIEMTargets
  FILE MIEMTargets.cmake
  NAMESPACE musica::
  DESTINATION lib/cmake/MIEM
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/MIEMConfig.cmake"
  DESTINATION lib/cmake/MIEM
)
