#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <future>
#include <git2_lib.hpp>
#include <pacman_lib.hpp>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// Fetch + filter each remote's branches asynchronously
std::unordered_map<std::string, std::vector<git2::RemoteBranch>>
fetch_remote_branches(const std::vector<App::Remote> &remotes) {
  using RemoteFetchResult =
      std::pair<std::string, std::vector<git2::RemoteBranch>>;
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
          return {remote.name, std::move(branches)};
        }));
  }

  std::unordered_map<std::string, std::vector<git2::RemoteBranch>>
      remote_branches;
  for (auto &task : fetch_tasks) {
    auto [name, branches] = task.get();
    remote_branches.emplace(std::move(name), std::move(branches));
  }
  return remote_branches;
}

} // namespace

int main(int argc, char **argv) {
  using namespace App;

  ensure_not_root();

  auto args_res = parse_args(std::span(argv, argc));
  if (!args_res) {
    return args_res.error();
  }
  auto args = *args_res;

  git_libgit2_init();

  // Set config to different one if set on args
  paths::config = args.config_dir;
  auto config = App::load_config(paths::config);
  std::println("Loaded config with {} remotes", config.remotes.size());

  auto &sync = args.command;
  std::println("sync: refresh={} upgrade={} packages={}", sync.refresh,
               sync.upgrade, sync.packages.size());
  for (auto &p : sync.packages) {
    std::println("  pkg: {}", p.unified());
  }

  auto remote_branches = fetch_remote_branches(config.remotes);
  auto installed_res = pacman::installed_packages();
  if (!installed_res) {
    std::println(stderr, "pacman -Q failed with exit code {}",
                 installed_res.error());
    git_libgit2_shutdown();
    return installed_res.error();
  }
  auto installed = *installed_res;
  std::println("pacman reports {} installed packages", installed.size());

  if (sync.refresh) {
    auto result = pacman::refresh();
    if (!result) {
      std::println(stderr, "pacman -Sy failed with exit code {}",
                   result.error());
      git_libgit2_shutdown();
      return result.error();
    }
    std::println("Refreshed package databases");
  }

  std::vector<std::string> package_names;
  package_names.reserve(sync.packages.size());
  for (auto &p : sync.packages) {
    package_names.push_back(p.name);
  }

  if (sync.upgrade || !package_names.empty()) {
    auto result = sync.upgrade
                      ? pacman::upgrade_and_install_packages(package_names)
                      : pacman::install_packages(package_names);
    if (!result) {
      std::println(stderr, "pacman sync failed with exit code {}",
                   result.error());
      git_libgit2_shutdown();
      return result.error();
    }
    std::println("pacman sync completed successfully");
  }

  git_libgit2_shutdown();
  return 0;
}
