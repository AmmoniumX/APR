#pragma once

#include <compare>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace pacman {

struct Package {
  std::string name;

  // A pacman version string: `[epoch:]pkgver[-pkgrel]`. pkgver may only
  // contain letters, digits, periods, and underscores (no hyphens, since
  // a hyphen separates pkgver from pkgrel). Ordering is delegated to
  // libalpm's alpm_pkg_vercmp, the same comparator pacman itself uses.
  struct Version {
    std::string value;

    std::strong_ordering operator<=>(const Version &other) const;
    bool operator==(const Version &other) const;
  };

  Version version;
};

// Runs makepkg sync install on the given directory. Assumes a PKGBUILD exists.
std::expected<void, int>
run_makepkg_sync_install(const std::filesystem::path &directory);

// The error type is the pacman/sudo exit code.
std::expected<std::vector<Package>, int> query_installed_packages();

// Refreshes the package databases via `sudo pacman -Sy --noconfirm`.
std::expected<void, int> refresh();

// Installs the given packages via `sudo pacman -S --noconfirm`.
std::expected<void, int> install_packages(std::span<std::string> packages);

// Refreshes databases, upgrades installed packages, and installs the given
// packages via `sudo pacman -Syu --noconfirm`.
std::expected<void, int>
upgrade_and_install_packages(std::span<std::string> packages);

} // namespace pacman
