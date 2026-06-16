################################################################################
# Tests

create_standard_test(
  NAME    nox_pipeline
  SOURCES test_nox_pipeline.cpp
  LIBRARIES miem_test_synthetic_nc
)

# Real-data pipeline test: runs against the committed CAMS BC subset,
# located via MIEM_TEST_DATA_DIR.
create_standard_test(
  NAME    bc_pipeline
  SOURCES test_bc_pipeline.cpp
)
target_compile_definitions(test_bc_pipeline PRIVATE
  MIEM_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/test/data"
)
