#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <git2_lib.hpp>
#include <print>
#include <span>

int main(int argc, char **argv) {
  auto args_res = App::parse_args(std::span(argv, argc));
  if (!args_res) {
    return args_res.error();
  }
  auto args = *args_res;

  git_libgit2_init();

  auto config = App::load_config(args.config_path);
  std::print("Loaded config with {} remotes and {} ignored packages\n",
             config.remotes.size(), config.ignored_packages.size());

  auto &sync = args.command;
  std::println("sync: refresh={} upgrade={} packages={}", sync.refresh,
               sync.upgrade, sync.packages.size());
  for (auto &p : sync.packages) {
    std::println("  pkg: {}", p.unified());
  }

  for (const auto &remote : config.remotes) {
    std::println("Fetching contents for remote {}", remote.name);
    for (const auto &branch_meta :
         git2::Repository::ls_remote_branches(remote.url.c_str())) {
      std::println("  {} : {}", branch_meta.name, branch_meta.commit_hash);
    }
  }

  git_libgit2_shutdown();
  return 0;
}
