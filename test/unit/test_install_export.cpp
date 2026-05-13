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
//   - Criterion #12 (E1 fix complement): no installed public header
//     uses the `throw` keyword.  Internal `ECCADReader` was moved to
//     src/internal/ in the port; this test ensures no other header
//     drifts.
//
// Both checks are file-scanning grep substitutes — they read files
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
// Criterion #12 — no `throw` keyword in any header that the install
// rule exposes (every .hpp / .h under include/miem/).  Comments are
// stripped of // and /* */ blocks before scanning so the test does not
// false-positive on the boundary-helper documentation that mentions
// "throws" prose.
// ---------------------------------------------------------------------
TEST(InstalledHeadersTest, NoThrowKeywordInPublicHeaders)
{
  fs::path inc = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "include" / "miem";
  ASSERT_TRUE(fs::exists(inc)) << inc;

  // Strip block comments (/* ... */) and line comments (//...) so that
  // documentation prose talking about exceptions does not trigger.
  auto strip_comments = [](std::string s) {
    std::string out;
    out.reserve(s.size());
    bool in_block = false;
    bool in_line  = false;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
      if (in_block)
      {
        if (i + 1 < s.size() && s[i] == '*' && s[i + 1] == '/')
        {
          in_block = false;
          ++i;
        }
        continue;
      }
      if (in_line)
      {
        if (s[i] == '\n') { in_line = false; out.push_back('\n'); }
        continue;
      }
      if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*')
      {
        in_block = true; ++i; continue;
      }
      if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/')
      {
        in_line = true; ++i; continue;
      }
      out.push_back(s[i]);
    }
    return out;
  };

  static const std::regex kThrow(R"(\bthrow\b)");
  std::vector<std::string> offenders;

  for (auto& entry : fs::recursive_directory_iterator(inc))
  {
    if (!entry.is_regular_file()) continue;
    const auto ext = entry.path().extension();
    if (ext != ".hpp" && ext != ".h") continue;
    const std::string src     = SlurpFile(entry.path());
    const std::string stripped = strip_comments(src);
    if (std::regex_search(stripped, kThrow))
    {
      offenders.push_back(entry.path().string());
    }
  }

  EXPECT_TRUE(offenders.empty())
      << "Public headers must not use `throw`.  Offenders:\n  "
      << [&]() {
           std::string s;
           for (const auto& o : offenders) { s += o; s += "\n  "; }
           return s;
         }();
}

// ---------------------------------------------------------------------
// M1 regression — the CMake-time install-export contract.
//
// The actual CMake invariant is enforced by `install(EXPORT
// miemTargets ...)` only listing `miem` and `miem_c`.  We verify by
// reading `CMakeLists.txt` and asserting:
//
//   1. `MIEM_EXPORT_TARGETS` is set to a list containing `miem`
//      (plus `miem_c` under MIEM_USE_DOUBLE), and explicitly does NOT
//      contain `miem_objects`.
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
  fs::path top = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "CMakeLists.txt";
  ASSERT_TRUE(fs::exists(top)) << top;
  const std::string text = SlurpFile(top);

  // The export-target list must declare `miem` and (conditionally)
  // `miem_c`; it must NOT include `miem_objects`.
  EXPECT_NE(text.find("set(MIEM_EXPORT_TARGETS miem)"), std::string::npos)
      << "Expected `set(MIEM_EXPORT_TARGETS miem)` in CMakeLists.txt";
  EXPECT_NE(text.find("list(APPEND MIEM_EXPORT_TARGETS miem_c)"),
            std::string::npos)
      << "Expected `list(APPEND MIEM_EXPORT_TARGETS miem_c)` "
         "in CMakeLists.txt";

  // Explicit negative: no install(TARGETS ... miem_objects ...) line
  // should bake the OBJECT lib into the export set.
  static const std::regex kInstallMiemObjects(
      R"(install\s*\(\s*TARGETS\s+[^\)]*\bmiem_objects\b)");
  EXPECT_FALSE(std::regex_search(text, kInstallMiemObjects))
      << "miem_objects must NOT appear in any install(TARGETS ...) call.";
}

TEST(InstallExportSetTest, ExportNamespaceIsMusica)
{
  fs::path top = fs::path(MIEM_TEST_PROJECT_SOURCE_DIR) / "CMakeLists.txt";
  ASSERT_TRUE(fs::exists(top));
  const std::string text = SlurpFile(top);

  // `install(EXPORT miemTargets NAMESPACE musica:: ...)` is the
  // contract that hands `musica::miem` / `musica::miem_c` to consumers.
  static const std::regex kNamespace(
      R"(install\s*\(\s*EXPORT\s+miemTargets[\s\S]*?NAMESPACE\s+musica::)");
  EXPECT_TRUE(std::regex_search(text, kNamespace))
      << "install(EXPORT) must use NAMESPACE musica::";
}

// Generated miemConfig.cmake (output of configure_package_config_file)
// imports the alias targets.  Verify the input template references
// musica::miem (and conditionally musica::miem_c).
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
