#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <future>
#include <git2_lib.hpp>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  using namespace App;

  auto args_res = App::parse_args(std::span(argv, argc));
  if (!args_res) {
    return args_res.error();
  }
  auto args = *args_res;

  git_libgit2_init();

  // Set config to different one if set on args
  paths::config = args.config_dir;
  auto config = App::load_config(paths::config);
  std::print("Loaded config with {} remotes and {} ignored packages\n",
             config.remotes.size(), config.ignored_packages.size());

  auto &sync = args.command;
  std::println("sync: refresh={} upgrade={} packages={}", sync.refresh,
               sync.upgrade, sync.packages.size());
  for (auto &p : sync.packages) {
    std::println("  pkg: {}", p.unified());
  }

  // Fetch + filter each remote's branches asynchronously
  using RemoteFetchResult =
      std::pair<std::string, std::vector<git2::RemoteBranch>>;
  std::vector<std::future<RemoteFetchResult>> fetch_tasks;
  fetch_tasks.reserve(config.remotes.size());
  for (const auto &remote : config.remotes) {
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

  std::println("Fetched remotes and branches:");
  for (const auto &[remote, branches] : remote_branches) {
    for (const auto &branch : branches) {
      std::println("  {}/{} ({})", remote, branch.name, branch.commit_hash);
    }
  }

  git_libgit2_shutdown();
  return 0;
}
