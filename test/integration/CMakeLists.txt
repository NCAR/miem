################################################################################
# Tests

create_standard_test(
  NAME    nox_pipeline
  SOURCES test_nox_pipeline.cpp
  LIBRARIES miem_test_synthetic_nc
)
