#include <gtest/gtest.h>
#include <netcdf.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "miem/emissions_module.hpp"
#include "miem/util/error.hpp"

using namespace miem;

namespace {

// RAII helper to create and clean up temp files
class TempDir {
 public:
  TempDir() : path_("miem_test_integration_XXXXXX") {
    // mkdtemp modifies the template in-place
    char tmpl[] = "/tmp/miem_test_integration_XXXXXX";
    char* result = mkdtemp(tmpl);
    if (!result) throw std::runtime_error("mkdtemp failed");
    path_ = result;
  }

  ~TempDir() {
    // Best-effort cleanup
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

// Create a minimal SES 1.0 compliant NetCDF file with known emission data.
void CreateTestNetCDF(const std::string& path,
                      int n_times, int n_cells,
                      const std::vector<double>& time_values,
                      const std::vector<std::string>& species,
                      const std::vector<std::vector<double>>& flux_data) {
  int ncid;
  int status = nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid);
  if (status != NC_NOERR)
    throw std::runtime_error("nc_create failed: " + std::string(nc_strerror(status)));

  // Global attributes
  nc_put_att_text(ncid, NC_GLOBAL, "ses_version", 3, "1.0");
  nc_put_att_text(ncid, NC_GLOBAL, "Conventions", 4, "CF-1.8");

  // Dimensions
  int time_dim, cell_dim;
  nc_def_dim(ncid, "time", static_cast<size_t>(n_times), &time_dim);
  nc_def_dim(ncid, "n_cells", static_cast<size_t>(n_cells), &cell_dim);

  // Time variable
  int time_varid;
  nc_def_var(ncid, "time", NC_DOUBLE, 1, &time_dim, &time_varid);
  const char* time_units = "seconds since 1970-01-01";
  nc_put_att_text(ncid, time_varid, "units",
                  std::strlen(time_units), time_units);

  // Species flux variables: (time, n_cells)
  std::vector<int> flux_varids(species.size());
  int dims2d[2] = {time_dim, cell_dim};
  for (size_t i = 0; i < species.size(); ++i) {
    std::string var_name = "emi_" + species[i];
    nc_def_var(ncid, var_name.c_str(), NC_DOUBLE, 2, dims2d, &flux_varids[i]);
  }

  nc_enddef(ncid);

  // Write time values
  nc_put_var_double(ncid, time_varid, time_values.data());

  // Write flux data for each species
  for (size_t i = 0; i < species.size(); ++i) {
    nc_put_var_double(ncid, flux_varids[i], flux_data[i].data());
  }

  nc_close(ncid);
}

void WriteSpeciesMapYAML(const std::string& path) {
  std::ofstream ofs(path);
  ofs << "species_map:\n"
      << "  mechanism: TEST\n"
      << "  mappings:\n"
      << "    - inventory: NOx\n"
      << "      mechanism: NO\n"
      << "      scaling: 0.9\n"
      << "    - inventory: NOx\n"
      << "      mechanism: NO2\n"
      << "      scaling: 0.1\n"
      << "    - inventory: SO2\n"
      << "      mechanism: SO2\n";
}

void WriteMIEMConfigYAML(const std::string& path,
                         const std::string& nc_path,
                         const std::string& species_map_path) {
  std::ofstream ofs(path);
  ofs << "miem:\n"
      << "  version: \"1.0\"\n"
      << "  sources:\n"
      << "    - name: test_source\n"
      << "      type: anthropogenic\n"
      << "      file_pattern: \"" << nc_path << "\"\n"
      << "      species_map: \"" << species_map_path << "\"\n"
      << "      temporal_interpolation: linear\n"
      << "      vertical_injection: surface\n";
}

void WriteMultiSourceConfig(const std::string& path,
                            const std::string& nc_path1,
                            const std::string& nc_path2,
                            const std::string& species_map_path) {
  std::ofstream ofs(path);
  ofs << "miem:\n"
      << "  version: \"1.0\"\n"
      << "  sources:\n"
      << "    - name: anthro_global\n"
      << "      type: anthropogenic\n"
      << "      file_pattern: \"" << nc_path1 << "\"\n"
      << "      species_map: \"" << species_map_path << "\"\n"
      << "      category: 1\n"
      << "      hierarchy: 1\n"
      << "      sector: anthropogenic\n"
      << "    - name: fire_global\n"
      << "      type: fire\n"
      << "      file_pattern: \"" << nc_path2 << "\"\n"
      << "      species_map: \"" << species_map_path << "\"\n"
      << "      category: 2\n"
      << "      hierarchy: 1\n"
      << "      sector: fire\n";
}

}  // namespace

class EmissionsIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tmp_ = std::make_unique<TempDir>();
    nc_path_ = tmp_->File("test_anthro.nc");
    species_map_path_ = tmp_->File("species_map.yaml");
    config_path_ = tmp_->File("config.yaml");

    // Two time steps: t=0s and t=86400s (one day apart)
    time_values_ = {0.0, 86400.0};

    // NOx flux: 1e-6 at t0, 2e-6 at t1 (uniform across cells)
    // SO2 flux: 3e-6 at t0, 4e-6 at t1
    nox_flux_.resize(n_times_ * n_cells_);
    so2_flux_.resize(n_times_ * n_cells_);
    for (int ic = 0; ic < n_cells_; ++ic) {
      nox_flux_[0 * n_cells_ + ic] = 1e-6;   // t0
      nox_flux_[1 * n_cells_ + ic] = 2e-6;   // t1
      so2_flux_[0 * n_cells_ + ic] = 3e-6;   // t0
      so2_flux_[1 * n_cells_ + ic] = 4e-6;   // t1
    }

    CreateTestNetCDF(nc_path_, n_times_, n_cells_,
                     time_values_, {"NOx", "SO2"},
                     {nox_flux_, so2_flux_});
    WriteSpeciesMapYAML(species_map_path_);
    WriteMIEMConfigYAML(config_path_, nc_path_, species_map_path_);
  }

  std::unique_ptr<TempDir> tmp_;
  std::string nc_path_;
  std::string species_map_path_;
  std::string config_path_;

  static constexpr int n_cells_ = 4;
  static constexpr int n_vert_levels_ = 2;
  static constexpr int n_times_ = 2;

  std::vector<double> time_values_;
  std::vector<double> nox_flux_;
  std::vector<double> so2_flux_;
};

TEST_F(EmissionsIntegrationTest, QuerySpeciesReturnsExpectedSet) {
  auto species = EmissionsModule::QuerySpecies(config_path_);

  // species_map splits NOx→NO,NO2 and passes SO2 through
  ASSERT_EQ(species.size(), 3u);

  // Sorted alphabetically (set-based collection)
  EXPECT_EQ(species[0], "NO");
  EXPECT_EQ(species[1], "NO2");
  EXPECT_EQ(species[2], "SO2");
}

TEST_F(EmissionsIntegrationTest, RunAtFirstTimestep) {
  EmissionsModule module(config_path_, n_cells_, n_vert_levels_);

  // Atmospheric state: uniform density and thickness
  const Real rho_val = 1.225;
  const Real dz_val = 100.0;
  std::vector<Real> air_density(n_vert_levels_ * n_cells_, rho_val);
  std::vector<Real> layer_thickness(n_vert_levels_ * n_cells_, dz_val);

  EmisState state = module.Run(0.0, air_density.data(),
                               layer_thickness.data(),
                               static_cast<int>(air_density.size()));

  EXPECT_EQ(state.n_species, 3);
  EXPECT_EQ(state.n_cells, n_cells_);
  EXPECT_EQ(state.n_vert_levels, n_vert_levels_);

  // Find species indices (sorted: NO, NO2, SO2)
  int no_idx = -1, no2_idx = -1, so2_idx = -1;
  for (int i = 0; i < static_cast<int>(state.species_names.size()); ++i) {
    if (state.species_names[i] == "NO") no_idx = i;
    if (state.species_names[i] == "NO2") no2_idx = i;
    if (state.species_names[i] == "SO2") so2_idx = i;
  }
  ASSERT_GE(no_idx, 0);
  ASSERT_GE(no2_idx, 0);
  ASSERT_GE(so2_idx, 0);

  // At t=0: NOx=1e-6, so NO=0.9e-6, NO2=0.1e-6, SO2=3e-6
  // Tendency = flux / (rho * dz)
  Real no_expected = 0.9e-6 / (rho_val * dz_val);
  Real no2_expected = 0.1e-6 / (rho_val * dz_val);
  Real so2_expected = 3e-6 / (rho_val * dz_val);

  // Check surface flux
  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(state.surface_flux[no_idx * n_cells_ + ic],
                0.9e-6, 1e-12);
    EXPECT_NEAR(state.surface_flux[no2_idx * n_cells_ + ic],
                0.1e-6, 1e-12);
    EXPECT_NEAR(state.surface_flux[so2_idx * n_cells_ + ic],
                3e-6, 1e-12);
  }

  // Check tendency at surface layer (layer 0, default injection)
  for (int ic = 0; ic < n_cells_; ++ic) {
    size_t no_tend = no_idx * n_vert_levels_ * n_cells_ + 0 * n_cells_ + ic;
    size_t no2_tend = no2_idx * n_vert_levels_ * n_cells_ + 0 * n_cells_ + ic;
    size_t so2_tend = so2_idx * n_vert_levels_ * n_cells_ + 0 * n_cells_ + ic;

    EXPECT_NEAR(state.tendency[no_tend], no_expected, 1e-18);
    EXPECT_NEAR(state.tendency[no2_tend], no2_expected, 1e-18);
    EXPECT_NEAR(state.tendency[so2_tend], so2_expected, 1e-18);

    // Layer 1 should be zero (surface injection only)
    size_t no_l1 = no_idx * n_vert_levels_ * n_cells_ + 1 * n_cells_ + ic;
    EXPECT_DOUBLE_EQ(state.tendency[no_l1], 0.0);
  }
}

TEST_F(EmissionsIntegrationTest, RunAtMidpoint) {
  EmissionsModule module(config_path_, n_cells_, n_vert_levels_);

  std::vector<Real> air_density(n_vert_levels_ * n_cells_, 1.225);
  std::vector<Real> layer_thickness(n_vert_levels_ * n_cells_, 100.0);

  // Midpoint: t=43200s (halfway between 0 and 86400)
  // Linear interpolation: NOx = (1e-6 + 2e-6)/2 = 1.5e-6
  // SO2 = (3e-6 + 4e-6)/2 = 3.5e-6
  EmisState state = module.Run(43200.0, air_density.data(),
                               layer_thickness.data(),
                               static_cast<int>(air_density.size()));

  int no_idx = -1, so2_idx = -1;
  for (int i = 0; i < static_cast<int>(state.species_names.size()); ++i) {
    if (state.species_names[i] == "NO") no_idx = i;
    if (state.species_names[i] == "SO2") so2_idx = i;
  }
  ASSERT_GE(no_idx, 0);
  ASSERT_GE(so2_idx, 0);

  // NO = 0.9 * 1.5e-6 = 1.35e-6
  // SO2 = 3.5e-6
  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(state.surface_flux[no_idx * n_cells_ + ic],
                1.35e-6, 1e-12);
    EXPECT_NEAR(state.surface_flux[so2_idx * n_cells_ + ic],
                3.5e-6, 1e-12);
  }
}

TEST_F(EmissionsIntegrationTest, ResolveHostIndices) {
  EmissionsModule module(config_path_, n_cells_, n_vert_levels_);

  // Simulate host model with species in a different order
  std::vector<std::string> host_species = {"CO2", "SO2", "O3", "NO", "NO2"};
  std::vector<int> indices;
  module.ResolveHostIndices(host_species, indices);

  // MIEM species (sorted): NO, NO2, SO2
  // Expected mappings: NO→3, NO2→4, SO2→1
  ASSERT_EQ(indices.size(), 3u);

  // Find which MIEM index maps to which host index
  for (size_t i = 0; i < indices.size(); ++i) {
    auto species_name = EmissionsModule::QuerySpecies(config_path_)[i];
    if (species_name == "NO") EXPECT_EQ(indices[i], 3);
    if (species_name == "NO2") EXPECT_EQ(indices[i], 4);
    if (species_name == "SO2") EXPECT_EQ(indices[i], 1);
  }
}

TEST_F(EmissionsIntegrationTest, MultiSourceCategorySummation) {
  // Create a second source file (fire) with SO2 only
  auto fire_nc = tmp_->File("test_fire.nc");
  auto multi_config = tmp_->File("multi_config.yaml");

  // Fire SO2: 1e-6 at both time steps
  std::vector<double> fire_so2(n_times_ * n_cells_);
  for (int i = 0; i < n_times_ * n_cells_; ++i) {
    fire_so2[i] = 1e-6;
  }

  // Fire NOx: 0.5e-6 at both time steps
  std::vector<double> fire_nox(n_times_ * n_cells_);
  for (int i = 0; i < n_times_ * n_cells_; ++i) {
    fire_nox[i] = 0.5e-6;
  }

  CreateTestNetCDF(fire_nc, n_times_, n_cells_,
                   time_values_, {"NOx", "SO2"},
                   {fire_nox, fire_so2});

  WriteMultiSourceConfig(multi_config, nc_path_, fire_nc,
                         species_map_path_);

  EmissionsModule module(multi_config, n_cells_, n_vert_levels_);
  std::vector<Real> rho(n_vert_levels_ * n_cells_, 1.225);
  std::vector<Real> dz(n_vert_levels_ * n_cells_, 100.0);

  auto state = module.Run(0.0, rho.data(), dz.data(),
                           static_cast<int>(rho.size()));

  // Anthro (cat=1): NOx=1e-6 → NO=0.9e-6, NO2=0.1e-6; SO2=3e-6
  // Fire   (cat=2): NOx=0.5e-6 → NO=0.45e-6, NO2=0.05e-6; SO2=1e-6
  // Sum across categories:
  //   NO  = 0.9e-6 + 0.45e-6 = 1.35e-6
  //   NO2 = 0.1e-6 + 0.05e-6 = 0.15e-6
  //   SO2 = 3e-6 + 1e-6 = 4e-6

  int no_idx = -1, no2_idx = -1, so2_idx = -1;
  for (int i = 0; i < static_cast<int>(state.species_names.size()); ++i) {
    if (state.species_names[i] == "NO") no_idx = i;
    if (state.species_names[i] == "NO2") no2_idx = i;
    if (state.species_names[i] == "SO2") so2_idx = i;
  }

  for (int ic = 0; ic < n_cells_; ++ic) {
    EXPECT_NEAR(state.surface_flux[no_idx * n_cells_ + ic], 1.35e-6, 1e-12);
    EXPECT_NEAR(state.surface_flux[no2_idx * n_cells_ + ic], 0.15e-6, 1e-12);
    EXPECT_NEAR(state.surface_flux[so2_idx * n_cells_ + ic], 4.0e-6, 1e-12);
  }

  // Verify sector fluxes populated
  EXPECT_TRUE(state.HasSectors());
  EXPECT_EQ(state.sector_names.size(), 2u);
}
