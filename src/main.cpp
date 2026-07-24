#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <Logger.hpp>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <future>
#include <git2_lib.hpp>
#include <pacman_lib.hpp>
#include <ranges>
#include <span>
#include <srcinfo.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class PackageAction { Install, Upgrade, Reinstall, None };

// Compares a package branch's .SRCINFO version against the currently
// installed version (if any) to decide whether it needs to be installed
// from scratch, upgraded, reinstalled, or left alone. An exact version match
// only triggers a reinstall if the package was explicitly named on the
// command line (as opposed to being pulled in via -u/--sysupgrade).
PackageAction determine_package_action(
    const srcinfo::SrcInfo &info, std::string_view pkg,
    std::optional<pacman::Package::Version> installed_version,
    bool explicitly_requested) {
  if (!installed_version.has_value()) {
    return PackageAction::Install;
  }
  pacman::Package::Version remote_version{info.version()};
  if (remote_version == *installed_version) {
    return explicitly_requested ? PackageAction::Reinstall
                                : PackageAction::None;
  }
  return remote_version > *installed_version ? PackageAction::Upgrade
                                             : PackageAction::None;
}

// Fetch + filter each remote's branches asynchronously
std::unordered_map<App::Remote, std::vector<git2::RemoteBranch>>
fetch_remote_branches(std::span<const App::Remote> remotes) {
  using RemoteFetchResult =
      std::pair<App::Remote, std::vector<git2::RemoteBranch>>;
  std::vector<std::future<RemoteFetchResult>> fetch_tasks;
  fetch_tasks.reserve(remotes.size());
  for (const auto &remote : remotes) {
    fetch_tasks.push_back(
        std::async(std::launch::async, [&remote]() -> RemoteFetchResult {
          auto branches =
              git2::Repository::ls_remote_branches(remote.url.c_str()) |
              // Filter out master and main branches from repositories
              std::views::filter([&remote](auto &branch) -> bool {
                return branch.name != std::string_view("master") &&
                       branch.name != std::string_view("main") &&
                       remote.matches(branch.name);
              }) |
              std::ranges::to<std::vector>();
          return {remote, std::move(branches)};
        }));
  }

  std::unordered_map<App::Remote, std::vector<git2::RemoteBranch>>
      remote_branches;
  for (auto &task : fetch_tasks) {
    auto [name, branches] = task.get();
    remote_branches.emplace(std::move(name), std::move(branches));
  }
  return remote_branches;
}

// Obtain list of packages to fetch within repositories
// We fetch packages that are either:
//   - Explicitly listed in the list of packages to sync
//   - The command includes the upgrade flag (-Su), and:
//     - It exists, locally, and:
//     - The remote it exists on has a higher priority than pacman
//     repositories (0)
std::vector<std::string> fetch_remote_packages(
    const App::SyncCommand &sync, std::span<const pacman::Package> installed,
    const std::unordered_map<App::Remote, std::vector<git2::RemoteBranch>>
        &remote_branches) {
  auto remote_pkg_names = remote_branches | std::views::values |
                          std::views::join |
                          std::views::transform(&git2::RemoteBranch::name);

  auto to_fetch = sync.packages | std::views::transform(&App::Package::name) |
                  std::views::filter([&](const auto &name) {
                    return std::ranges::contains(remote_pkg_names, name);
                  }) |
                  std::ranges::to<std::vector>();
  if (sync.upgrade) {
    auto in_local =
        installed | std::views::transform([](const auto &p) { return p.name; });
    auto in_remotes =
        remote_branches | std::views::filter([](const auto &rp) -> bool {
          return rp.first.priority > App::Remote::PACMAN_REPOS_PRIORITY;
        }) |
        std::views::values | std::views::join |
        std::views::transform(&git2::RemoteBranch::name);
    std::ranges::set_intersection(in_local, in_remotes,
                                  std::back_inserter(to_fetch));
  }
  return to_fetch;
}

namespace fs = std::filesystem;
} // namespace

namespace paths {
fs::path config;
fs::path cache;
} // namespace paths

int main(int argc, char **argv) {
  using namespace App;

  // Basic init
  ensure_not_root();
  auto args_res = parse_args(std::span(argv, argc));
  if (!args_res) {
    return args_res.error();
  }
  auto args = *args_res;
  Logger logger{args.verbose ? Logger::LogLevel::DEBUG
                             : Logger::LogLevel::INFO};
  git_libgit2_init();

  // Set config to different one if set on args
  paths::config = args.config_dir;
  paths::cache = get_cache_dir();
  // Extract remotes as const vector so we can refer pointers to it
  const auto remotes = [&]() {
    auto config = App::load_config(paths::config);
    return config.remotes;
  }();
  logger.info("Loaded config with {} remotes", remotes.size());

  auto &sync = args.command;
  logger.debug("sync: refresh={} upgrade={} packages={}", sync.refresh,
               sync.upgrade, sync.packages.size());
  if (logger.level() >= Logger::LogLevel::DEBUG) {
    for (auto &p : sync.packages) {
      logger.debug("   {}", p.unified());
    }
  }

  // Refresh if requested
  if (sync.refresh) {
    auto result = pacman::refresh();
    if (!result) {
      logger.error("pacman -Sy failed with exit code {}", result.error());
      git_libgit2_shutdown();
      return result.error();
    }
    logger.info("Refreshed package databases");
  }

  auto remote_branches = fetch_remote_branches(remotes);

  // Map each package name to the highest-priority remote offering it,
  // breaking ties alphabetically by remote name.
  StringMap<const Remote *> remotes_map{};
  for (const auto &[remote, branches] : remote_branches) {
    for (const auto &branch : branches) {
      auto [it, inserted] = remotes_map.try_emplace(branch.name, &remote);
      if (!inserted) {
        const auto &current = *it->second;
        if (remote.priority > current.priority ||
            (remote.priority == current.priority &&
             remote.name < current.name)) {
          it->second = &remote;
        }
      }
      logger.debug("Package {} offered by remote {} (priority {})", branch.name,
                   remote.name, remote.priority);
    }
  }

  auto installed_res = pacman::query_installed_packages();
  if (!installed_res) {
    logger.error("pacman -Q failed with exit code {}", installed_res.error());
    git_libgit2_shutdown();
    return installed_res.error();
  }
  auto installed = *installed_res;
  logger.info("pacman reports {} installed packages", installed.size());

  auto to_fetch = fetch_remote_packages(sync, installed, remote_branches);
  auto packages_not_in_remotes =
      sync.packages | std::views::filter([&](const auto &p) -> bool {
        return !std::ranges::contains(
            remote_branches | std::views::values | std::views::join |
                std::views::transform(&git2::RemoteBranch::name),
            p.name);
      }) |
      std::views::transform(&App::Package::name) |
      std::ranges::to<std::vector>();

  // === Main operation ===

  for (const auto &pkg : to_fetch) {
    logger.debug("Looking up package: {}", pkg);
    auto &remote = *remotes_map.at(pkg);
    auto *url = remote.url.c_str();
    auto path = paths::cache / remote.name / pkg;
    auto *branch = pkg.c_str();

    // Obtain repository, either opening and fetching existing or cloning new
    auto repository = [&]() -> git2::Repository {
      auto existing = git2::Repository::try_open(url, path, branch);
      if (existing) {
        logger.info("Fetching latest branch: {}/{}", remote.name, url);
        (void)existing->fetch_branch(branch);
        return std::move(*existing);
      }
      logger.info("Cloning new branch: {}/{}", remote.name, url);
      return git2::Repository::clone(url, path, branch);
    }();

    // Search for .SRCINFO and PKGBUILD in root directory
    if (!fs::is_regular_file(path / ".SRCINFO") ||
        !fs::is_regular_file(path / "PKGBUILD")) {
      logger.error("Repository branch {}/{} ({}) is not a valid package "
                   "branch, .SRCINFO and PKGBUILD files must exist",
                   remote.name, pkg, remote.url);
      return -1;
    }

    // Parse .SRCINFO and check if an update is required
    auto srcinfo_path = path / ".SRCINFO";
    std::ifstream srcinfo_file{srcinfo_path};
    std::stringstream srcinfo_contents;
    srcinfo_contents << srcinfo_file.rdbuf();
    auto info = srcinfo::SrcInfo::parse(srcinfo_contents.str());
    auto pkgver = info.version();

    bool explicitly_requested = std::ranges::contains(
        sync.packages | std::views::transform(&App::Package::name), pkg);

    auto do_install = [&]() -> std::expected<void, int> {
      return pacman::run_makepkg_sync_install(path);
    };

    auto installed_version = [&]() -> std::optional<pacman::Package::Version> {
      auto it = std::ranges::find(installed, pkg, &pacman::Package::name);
      if (it == installed.end()) {
        return std::nullopt;
      }
      return it->version;
    }();
    switch (determine_package_action(info, pkg, installed_version,
                                     explicitly_requested)) {
    case PackageAction::Install: {
      logger.info("Install: {} {}", pkg, pkgver);
      auto res = do_install();
      if (!res) {
        logger.error("Failed to run makepkg");
        return res.error();
      }
      break;
    }
    case PackageAction::Upgrade: {
      assert(installed_version.has_value());
      logger.info("Upgrade: {} {} -> {}", pkg, installed_version->value,
                  pkgver);
      auto res = do_install();
      if (!res) {
        logger.error("Failed to run makepkg");
        return res.error();
      }
      break;
    }
    case PackageAction::Reinstall: {
      logger.warn("Reinstall: {} {} == {}", pkg, installed_version->value,
                  pkgver);
      auto res = do_install();
      if (!res) {
        logger.error("Failed to run makepkg");
        return res.error();
      }
      break;
    }
    case PackageAction::None:
      // logger.debug("Package {} is up to date, skipping", pkg);
      break;
    }
  }

  if (sync.upgrade || !packages_not_in_remotes.empty()) {
    auto result =
        sync.upgrade
            ? pacman::upgrade_and_install_packages(packages_not_in_remotes)
            : pacman::install_packages(packages_not_in_remotes);
    if (!result) {
      logger.error("pacman sync failed with exit code {}", result.error());
      git_libgit2_shutdown();
      return result.error();
    }
    logger.info("pacman sync completed successfully");
  }

  git_libgit2_shutdown();
  return 0;
}
