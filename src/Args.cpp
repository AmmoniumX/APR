#include <Args.hpp>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <print>
#include <string>

namespace App {

std::expected<Args, int> parse_args(std::span<char *> args) {
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
