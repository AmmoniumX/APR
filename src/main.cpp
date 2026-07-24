#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <Logger.hpp>
#include <algorithm>
#include <future>
#include <git2_lib.hpp>
#include <pacman_lib.hpp>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

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
  auto to_fetch = sync.packages | std::views::transform(&App::Package::name) |
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

} // namespace

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
  // Extract remotes as const vector so we can refer pointers to it
  const auto remotes = [&]() {
    auto config = App::load_config(paths::config);
    return config.remotes;
  }();
  logger.info("Loaded config with {} remotes", remotes.size());
  StringMap<const Remote *> remotes_map{};
  for (const auto &r : remotes) {
    remotes_map.insert_or_assign(r.name, &r);
  }

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
  auto installed_res = pacman::installed_packages();
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

  std::vector<std::string> package_names;
  package_names.reserve(sync.packages.size());
  for (auto &p : sync.packages) {
    package_names.push_back(p.name);
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
