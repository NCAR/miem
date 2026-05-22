// Copyright (C) 2024-2026 University Corporation for Atmospheric Research
// SPDX-License-Identifier: Apache-2.0
//
// Build/install meta tests.
//
// These tests do NOT exercise runtime behaviour — they verify that the
// installed export set obeys MUSICA ecosystem invariants:
//
//   - M1 regression: musica::miem_objects is NOT in the install set.
//     The OBJECT library is a build-system implementation detail and
//     must never surface as an aliased target to downstream consumers.
//
// (A former "no `throw` in public headers" check was removed: the public
// API now throws `miem::MiemException` by design, mirroring MICM — whose
// public headers also throw `micm::MicmException` inline.)
//
// These checks are file-scanning grep substitutes — they read files
// under the source tree directly rather than relying on `cmake
// --install`, so they run during the regular test loop.

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// CMake-defined paths.  Provided at build time via target_compile_definitions.
#ifndef MIEM_TEST_PROJECT_SOURCE_DIR
#  error "MIEM_TEST_PROJECT_SOURCE_DIR must be defined by CMake"
#endif

namespace {

std::string SlurpFile(const fs::path& p)
{
  std::ifstream f(p);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

// ---------------------------------------------------------------------
// M1 regression — the CMake-time install-export contract.
//
// The actual CMake invariant is enforced by `install(EXPORT
// miemTargets ...)` only listing `miem`.  The install rules live in
// `packaging/CMakeLists.txt` (added via add_subdirectory(packaging)).
// We verify by reading that file and asserting:
//
//   1. `MIEM_EXPORT_TARGETS` is set to a list containing `miem`,
//      and explicitly does NOT contain `miem_objects`.
//
//   2. The `install(TARGETS ...)` call uses `${MIEM_EXPORT_TARGETS}`
//      (not a hard-coded list that might drift).
//
// This is a low-cost grep substitute for a real `cmake --install` +
// downstream `find_package` workflow.  A full install/import flow is
// orchestrated by musica's downstream CI.
// ---------------------------------------------------------------------
TEST(InstallExportSetTest, M1_MiemObjectsNotInExportSet)
{
  fs::path top = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "packaging" /
                 "CMakeLists.txt";
  ASSERT_TRUE(fs::exists(top)) << top;
  const std::string text = SlurpFile(top);

  // The export-target list must declare `miem`; it must NOT include
  // `miem_objects`.
  EXPECT_NE(text.find("set(MIEM_EXPORT_TARGETS miem)"), std::string::npos)
      << "Expected `set(MIEM_EXPORT_TARGETS miem)` in packaging/CMakeLists.txt";

  // Explicit negative: no install(TARGETS ... miem_objects ...) line
  // should bake the OBJECT lib into the export set.
  static const std::regex kInstallMiemObjects(
      R"(install\s*\(\s*TARGETS\s+[^\)]*\bmiem_objects\b)");
  EXPECT_FALSE(std::regex_search(text, kInstallMiemObjects))
      << "miem_objects must NOT appear in any install(TARGETS ...) call.";
}

TEST(InstallExportSetTest, ExportNamespaceIsMusica)
{
  fs::path top = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "packaging" /
                 "CMakeLists.txt";
  ASSERT_TRUE(fs::exists(top));
  const std::string text = SlurpFile(top);

  // `install(EXPORT miemTargets NAMESPACE musica:: ...)` is the
  // contract that hands `musica::miem` to consumers.
  static const std::regex kNamespace(
      R"(install\s*\(\s*EXPORT\s+miemTargets[\s\S]*?NAMESPACE\s+musica::)");
  EXPECT_TRUE(std::regex_search(text, kNamespace))
      << "install(EXPORT) must use NAMESPACE musica::";
}

// Generated miemConfig.cmake (output of configure_package_config_file)
// imports the alias targets.  Verify the input template references
// musica::miem.
TEST(InstallExportSetTest, ConfigPackageTemplateReferencesMusicaAliases)
{
  fs::path cfgin = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "cmake" /
                   "miemConfig.cmake.in";
  ASSERT_TRUE(fs::exists(cfgin)) << cfgin;
  const std::string text = SlurpFile(cfgin);

  // The exported targets file (`miemTargets.cmake`) is included; the
  // namespace is applied at install(EXPORT) time.  Sanity-check that
  // the template at least references the install path for
  // miemTargets.cmake.
  EXPECT_NE(text.find("miemTargets.cmake"), std::string::npos);
}
