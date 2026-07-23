#pragma once

#include <CLI/CLI.hpp>
#include <algorithm>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <vector>

#include <Config.hpp>
#include <Core.hpp>

namespace App {
namespace fs = std::filesystem;

struct SyncCommand {
  std::vector<Package> packages{}; // packages to install, e.g. `-S foo bar`
  bool refresh = false; // -y: fetch remotes before searching for packages
  bool upgrade = false; // -u: upgrade all installed packages
};

struct Args {
  fs::path config_dir = get_default_config();
  fs::path cache_dir = get_cache_dir();
  SyncCommand command;
};

inline std::expected<Args, int> parse_args(std::span<char *> args) {
  Args parsed_args;
  CLI::App app{"APRA - Arch Personal Repository Archive"};

  app.add_option("-c,--config", parsed_args.config_dir,
                 "Path to the configuration file");

  SyncCommand sync;
  bool do_sync = false;
  std::vector<std::string> package_args;
  auto *sync_opt = app.add_flag("-S,--sync", do_sync, "Synchronize packages");
  app.add_flag("-y,--refresh", sync.refresh,
               "Fetch remotes before searching for packages")
      ->needs(sync_opt);
  app.add_flag("-u,--sysupgrade", sync.upgrade,
               "Upgrade all installed packages")
      ->needs(sync_opt);
  app.add_option("packages", package_args,
                 "Packages to install, e.g. `-S foo bar`")
      ->needs(sync_opt);

  try {
    app.parse(static_cast<int>(args.size()), args.data());
  } catch (const CLI::ParseError &e) {
    return std::unexpected(app.exit(e));
  }

  if (do_sync) {
    sync.packages.resize(package_args.size());
    std::ranges::transform(package_args, sync.packages.begin(), Package::parse);
    parsed_args.command = sync;
  } else {
    std::println("No operation specified. Use --help for usage information.");
    return std::unexpected(1);
  }

  return parsed_args;
}

} // namespace App
