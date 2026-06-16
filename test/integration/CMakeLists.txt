################################################################################
# Tests

create_standard_test(
  NAME    nox_pipeline
  SOURCES test_nox_pipeline.cpp
  LIBRARIES miem_test_synthetic_nc
)

# Real-data pipeline test: runs against the committed CAMS BC subset
# (located via MIEM_TEST_DATA_DIR, set for every test by create_standard_test).
create_standard_test(
  NAME    bc_pipeline
  SOURCES test_bc_pipeline.cpp
)
