################################################################################
# Synthetic NetCDF test utility (lifted from feature/scaffolding).
#
# Linked into tests that need to generate ECCAD-conforming NetCDF
# fixtures or RAII temp directories.  Not installed.

add_library(miem_test_synthetic_nc STATIC
  synthetic_nc.cpp
)

target_include_directories(miem_test_synthetic_nc
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(miem_test_synthetic_nc
  PRIVATE netCDF::netcdf_normalized
)

target_compile_features(miem_test_synthetic_nc PUBLIC cxx_std_17)
