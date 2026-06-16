#include <miem/version.hpp>

#include <gtest/gtest.h>

#include <iostream>

TEST(Version, FullVersion)
{
  std::cout << "MIEM version: " << miem::GetMiemVersion() << std::endl;
}

TEST(Version, VersionMajor)
{
  std::cout << "MIEM version major: " << miem::GetMiemVersionMajor() << std::endl;
}

TEST(Version, VersionMinor)
{
  std::cout << "MIEM version minor: " << miem::GetMiemVersionMinor() << std::endl;
}

TEST(Version, VersionPatch)
{
  std::cout << "MIEM version patch: " << miem::GetMiemVersionPatch() << std::endl;
}

TEST(Version, VersionTweak)
{
  std::cout << "MIEM version tweak: " << miem::GetMiemVersionTweak() << std::endl;
}
