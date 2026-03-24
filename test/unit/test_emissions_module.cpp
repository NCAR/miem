#include <gtest/gtest.h>
#include <netcdf.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "miem/emissions_module.hpp"
#include "miem/util/error.hpp"

using namespace miem;

// Full emissions module tests require NetCDF test data + YAML config.
// These tests verify the interface and error handling.

TEST(EmissionsModule, QuerySpeciesMissingConfigThrows) {
  EXPECT_THROW(EmissionsModule::QuerySpecies("nonexistent.yaml"), ConfigError);
}

TEST(EmissionsModule, ConstructMissingConfigThrows) {
  EXPECT_THROW(EmissionsModule("nonexistent.yaml", 100, 10), ConfigError);
}

// --- Category/Hierarchy tests ---

namespace {

class TempDir {
 public:
  TempDir() {
    char tmpl[] = "/tmp/miem_test_cathier_XXXXXX";
    char* result = mkdtemp(tmpl);
    if (!result) throw std::runtime_error("mkdtemp failed");
    path_ = result;
  }
  ~TempDir() {
    std::string cmd = "rm -rf " + path_;
    std::system(cmd.c_str());
  }
  const std::string& Path() const { return path_; }
  std::string File(const std::string& name) const {
    return path_ + "/" + name;
  }
 private:
  std::string path_;
};

void CreateSimpleNetCDF(const std::string& path, int n_cells,
                         const std::vector<double>& nox_flux) {
  // Single time step file with emi_NOx(time, n_cells)
  int ncid;
  nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid);
  nc_put_att_text(ncid, NC_GLOBAL, "ses_version", 3, "1.0");

  int time_dim, cell_dim;
  nc_def_dim(ncid, "time", 1, &time_dim);
  nc_def_dim(ncid, "n_cells", static_cast<size_t>(n_cells), &cell_dim);

  int time_varid;
  nc_def_var(ncid, "time", NC_DOUBLE, 1, &time_dim, &time_varid);
  const char* units = "seconds since 1970-01-01";
  nc_put_att_text(ncid, time_varid, "units", std::strlen(units), units);

  int dims2d[2] = {time_dim, cell_dim};
  int nox_varid;
  nc_def_var(ncid, "emi_NOx", NC_DOUBLE, 2, dims2d, &nox_varid);

  nc_enddef(ncid);

  double time_val = 0.0;
  nc_put_var_double(ncid, time_varid, &time_val);
  nc_put_var_double(ncid, nox_varid, nox_flux.data());
  nc_close(ncid);
}

void WriteIdentitySpeciesMap(const std::string& path) {
  std::ofstream ofs(path);
  ofs << "species_map:\n"
      << "  mechanism: TEST\n"
      << "  mappings:\n"
      << "    - inventory: NOx\n"
      << "      mechanism: NOx\n";
}

void WriteConfig(const std::string& path,
                 const std::vector<std::tuple<std::string, std::string,
                     int, int, std::string, double>>& sources,
                 const std::string& species_map_path) {
  std::ofstream ofs(path);
  ofs << "miem:\n"
      << "  version: \"1.0\"\n"
      << "  sources:\n";
  for (const auto& [name, nc_path, cat, hier, sector, scale] : sources) {
    ofs << "    - name: " << name << "\n"
        << "      type: anthropogenic\n"
        << "      file_pattern: \"" << nc_path << "\"\n"
        << "      species_map: \"" << species_map_path << "\"\n"
        << "      category: " << cat << "\n"
        << "      hierarchy: " << hier << "\n";
    if (!sector.empty()) {
      ofs << "      sector: " << sector << "\n";
    }
    if (scale != 1.0) {
      ofs << "      scaling_factor: " << scale << "\n";
    }
  }
}

}  // namespace

class CategoryHierarchyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tmp_ = std::make_unique<TempDir>();
    species_map_path_ = tmp_->File("species_map.yaml");
    WriteIdentitySpeciesMap(species_map_path_);
  }

  std::unique_ptr<TempDir> tmp_;
  std::string species_map_path_;

  static constexpr int n_cells_ = 4;
  static constexpr int n_vert_ = 1;
};

TEST_F(CategoryHierarchyTest, BasicSum) {
  // Two sources in different categories → summed
  auto nc1 = tmp_->File("src1.nc");
  auto nc2 = tmp_->File("src2.nc");
  auto cfg = tmp_->File("config.yaml");

  std::vector<double> flux1(n_cells_, 1.0e-6);
  std::vector<double> flux2(n_cells_, 2.0e-6);
  CreateSimpleNetCDF(nc1, n_cells_, flux1);
  CreateSimpleNetCDF(nc2, n_cells_, flux2);

  WriteConfig(cfg, {
      {"src1", nc1, 1, 1, "anthro", 1.0},
      {"src2", nc2, 2, 1, "fire",   1.0},
  }, species_map_path_);

  EmissionsModule module(cfg, n_cells_, n_vert_);
  std::vector<Real> rho(n_vert_ * n_cells_, 1.0);
  std::vector<Real> dz(n_vert_ * n_cells_, 1.0);

  auto state = module.Run(0.0, rho.data(), dz.data(),
                           static_cast<int>(rho.size()));

  // Different categories sum: 1e-6 + 2e-6 = 3e-6
  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(state.surface_flux[ic], 3.0e-6, 1e-12);
  }
}

TEST_F(CategoryHierarchyTest, HierarchyOverride) {
  // Two sources in same category, different hierarchy → higher wins
  auto nc_global = tmp_->File("global.nc");
  auto nc_regional = tmp_->File("regional.nc");
  auto cfg = tmp_->File("config.yaml");

  std::vector<double> global_flux(n_cells_, 1.0e-6);
  // Regional has data in cells 0,1 only; cells 2,3 are zero
  std::vector<double> regional_flux = {5.0e-6, 5.0e-6, 0.0, 0.0};
  CreateSimpleNetCDF(nc_global, n_cells_, global_flux);
  CreateSimpleNetCDF(nc_regional, n_cells_, regional_flux);

  WriteConfig(cfg, {
      {"global",   nc_global,   1, 1, "", 1.0},
      {"regional", nc_regional, 1, 2, "", 1.0},
  }, species_map_path_);

  EmissionsModule module(cfg, n_cells_, n_vert_);
  std::vector<Real> rho(n_vert_ * n_cells_, 1.0);
  std::vector<Real> dz(n_vert_ * n_cells_, 1.0);

  auto state = module.Run(0.0, rho.data(), dz.data(),
                           static_cast<int>(rho.size()));

  // Cells 0,1: regional (hierarchy=2) wins with 5e-6
  // Cells 2,3: regional has zero, so global (hierarchy=1) provides 1e-6
  EXPECT_NEAR(state.surface_flux[0], 5.0e-6, 1e-12);
  EXPECT_NEAR(state.surface_flux[1], 5.0e-6, 1e-12);
  EXPECT_NEAR(state.surface_flux[2], 1.0e-6, 1e-12);
  EXPECT_NEAR(state.surface_flux[3], 1.0e-6, 1e-12);
}

TEST_F(CategoryHierarchyTest, ScalingFactor) {
  // Source with scaling_factor: 0.5 → output halved
  auto nc = tmp_->File("src.nc");
  auto cfg = tmp_->File("config.yaml");

  std::vector<double> flux(n_cells_, 4.0e-6);
  CreateSimpleNetCDF(nc, n_cells_, flux);

  WriteConfig(cfg, {
      {"scaled", nc, 1, 1, "anthro", 0.5},
  }, species_map_path_);

  EmissionsModule module(cfg, n_cells_, n_vert_);
  std::vector<Real> rho(n_vert_ * n_cells_, 1.0);
  std::vector<Real> dz(n_vert_ * n_cells_, 1.0);

  auto state = module.Run(0.0, rho.data(), dz.data(),
                           static_cast<int>(rho.size()));

  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(state.surface_flux[ic], 2.0e-6, 1e-12);
  }
}

TEST_F(CategoryHierarchyTest, SectorFluxExposure) {
  // Verify sector_fluxes populated alongside aggregated surface_flux
  auto nc1 = tmp_->File("anthro.nc");
  auto nc2 = tmp_->File("fire.nc");
  auto cfg = tmp_->File("config.yaml");

  std::vector<double> flux1(n_cells_, 1.0e-6);
  std::vector<double> flux2(n_cells_, 3.0e-6);
  CreateSimpleNetCDF(nc1, n_cells_, flux1);
  CreateSimpleNetCDF(nc2, n_cells_, flux2);

  WriteConfig(cfg, {
      {"anthro_src", nc1, 1, 1, "anthropogenic", 1.0},
      {"fire_src",   nc2, 2, 1, "fire",          1.0},
  }, species_map_path_);

  EmissionsModule module(cfg, n_cells_, n_vert_);
  std::vector<Real> rho(n_vert_ * n_cells_, 1.0);
  std::vector<Real> dz(n_vert_ * n_cells_, 1.0);

  auto state = module.Run(0.0, rho.data(), dz.data(),
                           static_cast<int>(rho.size()));

  EXPECT_TRUE(state.HasSectors());
  ASSERT_EQ(state.sector_names.size(), 2u);

  const auto& anthro_flux = state.GetSectorFlux("anthropogenic");
  const auto& fire_flux = state.GetSectorFlux("fire");

  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(anthro_flux[ic], 1.0e-6, 1e-12);
    EXPECT_NEAR(fire_flux[ic], 3.0e-6, 1e-12);
    // Aggregated should be sum of categories
    EXPECT_NEAR(state.surface_flux[ic], 4.0e-6, 1e-12);
  }
}
