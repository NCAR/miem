#include <gtest/gtest.h>
#include <netcdf.h>

#include <cstring>
#include <string>
#include <vector>

#include "miem/ses_reader.hpp"
#include "miem/util/error.hpp"

using namespace miem;

// SESReader tests require NetCDF test data files.
// These tests verify the interface; full I/O tests need generated test data.

TEST(SESReader, DefaultState) {
  SESReader reader;
  EXPECT_FALSE(reader.IsOpen());
  EXPECT_EQ(reader.NumCells(), 0);
  EXPECT_EQ(reader.NumTimeSteps(), 0);
}

TEST(SESReader, OpenNonexistentFileThrows) {
  SESReader reader;
  EXPECT_THROW(reader.Open("nonexistent_file.nc"), IOError);
}

TEST(SESReader, ReadFluxWithoutOpenThrows) {
  SESReader reader;
  std::vector<Real> flux;
  int n_cells;
  EXPECT_THROW(reader.ReadFlux(0, {"NO"}, flux, n_cells), IOError);
}

// --- SES Detection Tests ---

namespace {

class TempDir {
 public:
  TempDir() {
    char tmpl[] = "/tmp/miem_test_ses_XXXXXX";
    char* result = mkdtemp(tmpl);
    if (!result) throw std::runtime_error("mkdtemp failed");
    path_ = result;
  }
  ~TempDir() {
    std::string cmd = "rm -rf " + path_;
    std::system(cmd.c_str());
  }
  std::string File(const std::string& name) const {
    return path_ + "/" + name;
  }
 private:
  std::string path_;
};

void CreateSESFile(const std::string& path, int n_cells = 4) {
  int ncid;
  nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid);
  nc_put_att_text(ncid, NC_GLOBAL, "ses_version", 3, "1.0");
  nc_put_att_text(ncid, NC_GLOBAL, "Conventions", 4, "CF-1.8");

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

  std::vector<double> flux(n_cells, 1.0e-6);
  nc_put_var_double(ncid, nox_varid, flux.data());
  nc_close(ncid);
}

void CreateLegacyCESFile(const std::string& path, int n_cells = 4) {
  int ncid;
  nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid);
  nc_put_att_text(ncid, NC_GLOBAL, "miem_version", 3, "1.0");

  int time_dim, cell_dim;
  nc_def_dim(ncid, "Time", 1, &time_dim);
  nc_def_dim(ncid, "nCells", static_cast<size_t>(n_cells), &cell_dim);

  int time_varid;
  nc_def_var(ncid, "Time", NC_DOUBLE, 1, &time_dim, &time_varid);
  const char* units = "seconds since 1970-01-01";
  nc_put_att_text(ncid, time_varid, "units", std::strlen(units), units);

  int dims2d[2] = {time_dim, cell_dim};
  int nox_varid;
  nc_def_var(ncid, "emi_NOx", NC_DOUBLE, 2, dims2d, &nox_varid);

  nc_enddef(ncid);

  double time_val = 0.0;
  nc_put_var_double(ncid, time_varid, &time_val);

  std::vector<double> flux(n_cells, 2.0e-6);
  nc_put_var_double(ncid, nox_varid, flux.data());
  nc_close(ncid);
}

}  // namespace

TEST(SESReader, SESCompliantDetection) {
  TempDir tmp;
  auto path = tmp.File("ses_test.nc");
  CreateSESFile(path);

  SESReader reader;
  reader.Open(path);
  EXPECT_TRUE(reader.IsOpen());
  EXPECT_EQ(reader.NumCells(), 4);
  EXPECT_EQ(reader.NumTimeSteps(), 1);

  auto species = reader.QuerySpecies();
  ASSERT_EQ(species.size(), 1u);
  EXPECT_EQ(species[0], "NOx");

  // Read flux and verify
  std::vector<Real> flux;
  int n_cells_out;
  reader.ReadFlux(0, {"NOx"}, flux, n_cells_out);
  EXPECT_EQ(n_cells_out, 4);
  for (int ic = 0; ic < 4; ++ic) {
    EXPECT_NEAR(flux[ic], 1.0e-6, 1e-12);
  }
}

TEST(SESReader, LegacyMIEMVersionFallback) {
  TempDir tmp;
  auto path = tmp.File("legacy_test.nc");
  CreateLegacyCESFile(path);

  SESReader reader;
  reader.Open(path);
  EXPECT_TRUE(reader.IsOpen());
  EXPECT_EQ(reader.NumCells(), 4);
  EXPECT_EQ(reader.NumTimeSteps(), 1);

  auto species = reader.QuerySpecies();
  ASSERT_EQ(species.size(), 1u);
  EXPECT_EQ(species[0], "NOx");

  std::vector<Real> flux;
  int n_cells_out;
  reader.ReadFlux(0, {"NOx"}, flux, n_cells_out);
  EXPECT_EQ(n_cells_out, 4);
  for (int ic = 0; ic < 4; ++ic) {
    EXPECT_NEAR(flux[ic], 2.0e-6, 1e-12);
  }
}
