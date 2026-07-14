#pragma once

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <Config.hpp>
#include <Core.hpp>

namespace App {
namespace fs = std::filesystem;

namespace Commands {
struct FetchRemotes {
  std::vector<std::string> remotes; // Or All
};
struct InstallPackages {
  std::vector<Package> packages{}; // list of packages to install
  bool fetch = true;   // if true, fetch remotes before searching for packages
  bool upgrade = true; // if true, also update all packages with updates
};
} // namespace Commands

using Command = std::variant<Commands::FetchRemotes, Commands::InstallPackages>;

struct Args {
  fs::path config_path = get_default_config_path();
  Command command;
};

inline std::expected<Args, int> parse_args(std::span<char *> args) {
  Args parsed_args;
  CLI::App app{"APR - Arch Personal Repository"};

  app.add_option("-c,--config", parsed_args.config_path,
                 "Path to the configuration file");

  Commands::FetchRemotes fetch;
  auto fetch_cmd =
      app.add_subcommand("fetch", "Fetch all remotes or specific ones");
  fetch_cmd->add_option("remotes", fetch.remotes,
                        "List of remotes to fetch (optional)");

  Commands::InstallPackages install;
  auto install_cmd =
      app.add_subcommand("install", "Install or upgrade a specific package");

  install_cmd->add_flag(
      "--fetch,!--no-fetch", install.fetch,
      "Fetch remotes before searching for packages to install");
  install_cmd->add_flag("--upgrade,!--no-upgrade", install.upgrade,
                        "Upgrade all existing packages");
  install_cmd->add_option("packages", install.packages,
                          "List of packages to install");

  try {
    app.parse(static_cast<int>(args.size()), args.data());
  } catch (const CLI::ParseError &e) {
    return std::unexpected(app.exit(e));
  }

  if (*fetch_cmd) {
    parsed_args.command = fetch;
  } else if (*install_cmd) {
    parsed_args.command = install;
  } else {
    std::println("No command provided. Use --help for usage information.");
    return std::unexpected(1);
  }

  return parsed_args;
}

} // namespace App
