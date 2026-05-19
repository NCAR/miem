// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Smoke tests for the C API.  Builds a config programmatically through
// the setters, runs the lifecycle, and verifies the H4 error-category
// mapping at the boundary.

#include "synthetic_nc.hpp"

#include <miem/miem_c.h>
#include <miem/util/error.hpp>
#include <miem/util/result.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

using miem_test::CreateTestNetCDF;
using miem_test::TempDir;

namespace {

// Write a 2-time-step single-species file and return its path.
std::string MakeNetCDF(const TempDir& dir, int n_cells, double flux,
                       const std::string& name = "ok.nc",
                       const std::string& species = "NOx")
{
  const std::string path = dir.File(name);
  std::vector<double> data(2 * n_cells, flux);
  CreateTestNetCDF(path, /*n_times=*/2, n_cells,
                   /*time_values=*/{ 0.0, 3600.0 },
                   /*species=*/{ species }, /*flux_data=*/{ data });
  return path;
}

// Build a valid miem_source_spec_t with default-ish ECCAD offline values.
//
// IMPORTANT: the returned spec aliases the caller's strings via the
// `const char*` fields.  Callers must keep `name`, `file_pattern`, and
// `sector` alive until they have finished using the spec (typically
// until after `miem_config_add_source` returns, which copies the
// string contents internally).  We deliberately do NOT bind to
// temporaries here — every parameter is taken by `const char*` so the
// caller owns lifetime explicitly.
miem_source_spec_t MakeSpec(const char* name,
                            const char* file_pattern,
                            int category = 0, int hierarchy = 1,
                            const char* sector = nullptr)
{
  miem_source_spec_t spec{};
  spec.name                   = name;
  spec.mode                   = 0;  // offline
  spec.type                   = MIEM_SOURCE_TYPE_ANTHROPOGENIC;
  spec.file_pattern           = file_pattern;
  spec.convention             = "eccad";
  spec.temporal_interpolation = 0;  // linear
  spec.vertical_injection     = 0;  // surface
  spec.category               = category;
  spec.hierarchy              = hierarchy;
  spec.scaling_factor         = 1.0;
  spec.sector                 = sector;
  return spec;
}

}  // namespace

// ---------------------------------------------------------------------
// C1 — Full lifecycle.  miem_config_new -> setters -> validate ->
// CreateMIEM -> MIEMRun -> accessors -> Delete*
// ---------------------------------------------------------------------
TEST(MIEMCApiTest, C1_FullLifecycle)
{
  TempDir dir;
  const int n_cells = 4;
  const std::string path = MakeNetCDF(dir, n_cells, 1.0e-9);
  const std::string spec_name = "cams";

  miem_config_t* cfg = miem_config_new();
  ASSERT_NE(cfg, nullptr);
  miem_config_set_regridding_none(cfg);

  miem_source_spec_t spec =
      MakeSpec(spec_name.c_str(), path.c_str(), 0, 1, "anthropogenic");

  MIEM_Error err{};
  ASSERT_EQ(miem_config_add_source(cfg, &spec, &err), 0);

  // Add species mapping NOx -> NO 1.0 (identity-after-rename).
  ASSERT_EQ(miem_config_add_species_mapping(
                cfg, spec_name.c_str(), "NOx", "NO", 1.0, &err),
            0);

  ASSERT_EQ(miem_config_validate(cfg, &err), 0)
      << "validate err: code=" << err.code << " msg=" << err.message;

  miem_t* handle = nullptr;
  ASSERT_EQ(CreateMIEM(cfg, n_cells, /*n_vert_levels=*/2, &handle, &err), 0)
      << "CreateMIEM err: " << err.message;
  ASSERT_NE(handle, nullptr);

  miem_state_t* state = nullptr;
  ASSERT_EQ(MIEMRun(handle, /*time_sec=*/1800.0, /*dt=*/60.0,
                    /*air_density=*/nullptr,
                    /*layer_thickness=*/nullptr,
                    /*n_atm=*/0,
                    &state, &err),
            0)
      << "MIEMRun err: " << err.message;
  ASSERT_NE(state, nullptr);

  EXPECT_EQ(MIEMGetStateNumSpecies(state), 1);
  EXPECT_EQ(MIEMGetStateNumCells(state),  n_cells);

  double* sf = MIEMGetSurfaceFlux(state);
  ASSERT_NE(sf, nullptr);
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(sf[ic], 1.0e-9, 1.0e-22);
  }

  EXPECT_EQ(MIEMGetSectorCount(state), 1);
  char name_buf[MIEM_MAX_NAME_LEN] = { 0 };
  EXPECT_EQ(MIEMGetSectorName(state, 0, name_buf, &err), 0);
  EXPECT_STREQ(name_buf, "anthropogenic");

  double* sec_flux = MIEMGetSectorFlux(state, "anthropogenic", &err);
  ASSERT_NE(sec_flux, nullptr);
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_NEAR(sec_flux[ic], 1.0e-9, 1.0e-22);
  }

  DeleteMIEMState(state);
  DeleteMIEM(handle);
  miem_config_delete(cfg);
}

// ---------------------------------------------------------------------
// C2 — MIEMGetSurfaceFlux returns bit-equal values to C++ EmissionsState
// (well, the same values, since the underlying buffer is shared via
// the opaque handle).
// ---------------------------------------------------------------------
TEST(MIEMCApiTest, C2_SurfaceFluxBitEqualToCppPath)
{
  TempDir dir;
  const int n_cells = 3;
  const std::string path = MakeNetCDF(dir, n_cells, 7.0e-9);

  miem_config_t* cfg = miem_config_new();
  miem_source_spec_t spec = MakeSpec("only", path.c_str());
  MIEM_Error err{};
  miem_config_add_source(cfg, &spec, &err);
  miem_config_add_species_mapping(cfg, "only", "NOx", "NO", 1.0, &err);

  miem_t* handle = nullptr;
  ASSERT_EQ(CreateMIEM(cfg, n_cells, 2, &handle, &err), 0);

  miem_state_t* state = nullptr;
  ASSERT_EQ(MIEMRun(handle, 1800.0, 60.0, nullptr, nullptr, 0, &state, &err),
            0);

  // Every cell should be 7e-9 because identity mapping NOx -> NO.
  double* sf = MIEMGetSurfaceFlux(state);
  for (int ic = 0; ic < n_cells; ++ic)
  {
    EXPECT_EQ(sf[ic], 7.0e-9);  // bit-equal: no math performed on it
  }

  DeleteMIEMState(state);
  DeleteMIEM(handle);
  miem_config_delete(cfg);
}

// ---------------------------------------------------------------------
// C3 — Invalid config -> CreateMIEM returns nonzero, MIEM_Error code is
// populated and the category enum matches the H4 mapping (not all
// collapsed to InternalError).
// ---------------------------------------------------------------------
TEST(MIEMCApiTest, C3_InvalidConfigCreateMIEMFails)
{
  miem_config_t* cfg = miem_config_new();
  miem_source_spec_t spec = MakeSpec("bad", "/tmp/no.nc");
  spec.mode = 1;   // online -> OnlineSourcesNotSupported
  MIEM_Error err{};
  miem_config_add_source(cfg, &spec, &err);

  miem_t* handle = nullptr;
  int rc = CreateMIEM(cfg, 4, 2, &handle, &err);
  EXPECT_NE(rc, 0);
  EXPECT_EQ(handle, nullptr);
  // H4 fix: code is the OnlineSourcesNotSupported enum, NOT
  // InternalError.
  EXPECT_EQ(err.code,
            static_cast<int>(miem::ErrorCode::OnlineSourcesNotSupported));

  miem_config_delete(cfg);
}

// H4 mapping coverage: ConfigInvalid path (null cfg).
TEST(MIEMCApiTest, H4_ConfigInvalidCategoryDistinct)
{
  miem_t* handle = nullptr;
  MIEM_Error err{};
  int rc = CreateMIEM(nullptr, 4, 2, &handle, &err);
  EXPECT_NE(rc, 0);
  EXPECT_EQ(err.code, static_cast<int>(miem::ErrorCode::ConfigInvalid));
}

// ---------------------------------------------------------------------
// C4 — MIEMGetSectorFlux with unknown sector -> NULL, populates Error.
// ---------------------------------------------------------------------
TEST(MIEMCApiTest, C4_UnknownSectorReturnsNullAndPopulatesError)
{
  TempDir dir;
  const int n_cells = 3;
  const std::string path = MakeNetCDF(dir, n_cells, 1.0e-9);

  miem_config_t* cfg = miem_config_new();
  miem_source_spec_t spec = MakeSpec("s", path.c_str(), 0, 1, "anthropogenic");
  MIEM_Error err{};
  miem_config_add_source(cfg, &spec, &err);
  miem_config_add_species_mapping(cfg, "s", "NOx", "NO", 1.0, &err);

  miem_t* handle = nullptr;
  ASSERT_EQ(CreateMIEM(cfg, n_cells, 2, &handle, &err), 0);

  miem_state_t* state = nullptr;
  ASSERT_EQ(MIEMRun(handle, 1800.0, 60.0, nullptr, nullptr, 0, &state, &err),
            0);

  double* unknown = MIEMGetSectorFlux(state, "nonexistent", &err);
  EXPECT_EQ(unknown, nullptr);
  EXPECT_EQ(err.code, static_cast<int>(miem::ErrorCode::UnknownSector));

  DeleteMIEMState(state);
  DeleteMIEM(handle);
  miem_config_delete(cfg);
}

// Round-trip with air_density + layer_thickness populates tendency_.
TEST(MIEMCApiTest, RunWithAtmospherePopulatesTendency)
{
  TempDir dir;
  const int n_cells = 3;
  const int n_vl    = 2;
  const std::string path = MakeNetCDF(dir, n_cells, 1.0e-9);

  miem_config_t* cfg = miem_config_new();
  miem_source_spec_t spec = MakeSpec("s", path.c_str());
  MIEM_Error err{};
  miem_config_add_source(cfg, &spec, &err);
  miem_config_add_species_mapping(cfg, "s", "NOx", "NO", 1.0, &err);

  miem_t* handle = nullptr;
  ASSERT_EQ(CreateMIEM(cfg, n_cells, n_vl, &handle, &err), 0);

  std::vector<double> rho(n_vl * n_cells, 1.225);
  std::vector<double> dz (n_vl * n_cells, 100.0);

  miem_state_t* state = nullptr;
  ASSERT_EQ(MIEMRun(handle, 1800.0, 60.0,
                    rho.data(), dz.data(), n_vl * n_cells,
                    &state, &err),
            0)
      << err.message;

  double* tend = MIEMGetTendency(state);
  ASSERT_NE(tend, nullptr);

  // Surface injection layer (0) should hold flux / (rho*dz)
  for (int ic = 0; ic < n_cells; ++ic)
  {
    // (species=0, level=0, cell=ic) -> index ic
    EXPECT_NEAR(tend[ic], 1.0e-9 / (1.225 * 100.0), 1.0e-25);
  }

  DeleteMIEMState(state);
  DeleteMIEM(handle);
  miem_config_delete(cfg);
}
