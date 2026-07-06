################################################################################
# Test utilities

include(${PROJECT_SOURCE_DIR}/cmake/test_util.cmake)

################################################################################
# Disable clang tidy for test files
set(CMAKE_CXX_CLANG_TIDY "")

add_subdirectory(util)
add_subdirectory(unit)
add_subdirectory(tutorial)
add_subdirectory(integration)
