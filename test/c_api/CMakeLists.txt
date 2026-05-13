################################################################################
# C API smoke tests.  Only build when the C API target exists (it's
# gated by MIEM_USE_DOUBLE=ON; the C API is unconditionally double).

if(TARGET miem_c)
  add_executable(test_miem_c test_miem_c.cpp)
  target_link_libraries(test_miem_c
    PRIVATE miem_c miem_test_synthetic_nc GTest::gtest_main
  )
  # Need access to miem/util/error.hpp for ErrorCode enum comparisons
  # — the C API surface exposes the integer code but tests pin it
  # against the C++ enum to assert H4 boundary mapping behaviour.
  target_include_directories(test_miem_c
    PRIVATE ${PROJECT_SOURCE_DIR}/include
  )
  add_test(NAME miem_c
           COMMAND test_miem_c
           WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()
