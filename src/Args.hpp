#pragma once

#include <CLI11.hpp>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace App {
namespace fs = std::filesystem;

inline fs::path default_config_dir() {
  if (const char *xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
    return fs::path(xdg_config_home);
  } else if (const char *home = std::getenv("HOME")) {
    return fs::path(home) / ".config";
  } else {
    throw std::runtime_error(
        "Neither XDG_CONFIG_HOME nor HOME environment variables are set");
  }
}

namespace Commands {
struct FetchRemotes {
  std::vector<std::string> remotes; // Or All
};
struct UpgradeRemotes {
  std::vector<std::string> remotes; // Or All
};
struct AddPackage {
  std::string package_name;
  std::string remote; // If empty, search all remotes
};
} // namespace Commands

using Command = std::variant<Commands::FetchRemotes, Commands::UpgradeRemotes,
                             Commands::AddPackage>;

struct Args {
  fs::path config_path = default_config_dir() / "apr" / "config.json";
  Command command;
};

inline std::expected<Args, int> parse_args(std::span<char *> args) {
  Args parsed_args;
  CLI::App app{"APR - Arch Personal Repository"};

  app.add_option("-c,--config", parsed_args.config_path,
                 "Path to the configuration file");

  auto fetch_remotes_cmd =
      app.add_subcommand("fetch-remotes", "Fetch all remotes or specific ones");
  std::vector<std::string> fetch_remotes;
  fetch_remotes_cmd->add_option("remotes", fetch_remotes,
                                "List of remotes to fetch (optional)");

  auto upgrade_remotes_cmd = app.add_subcommand(
      "upgrade-remotes", "Upgrade all remotes or specific ones");
  std::vector<std::string> upgrade_remotes;
  upgrade_remotes_cmd->add_option("remotes", upgrade_remotes,
                                  "List of remotes to upgrade (optional)");

  auto add_package_cmd =
      app.add_subcommand("add-package", "Add a package from a remote");
  std::string package_name;
  std::string remote;
  add_package_cmd
      ->add_option("package_name", package_name, "Name of the package to add")
      ->required();
  add_package_cmd->add_option("--remote", remote,
                              "Remote from which to add the package");

  try {
    app.parse(static_cast<int>(args.size()), args.data());
  } catch (const CLI::ParseError &e) {
    return std::unexpected(app.exit(e));
  }

  if (*fetch_remotes_cmd) {
    parsed_args.command = Commands::FetchRemotes{fetch_remotes};
  } else if (*upgrade_remotes_cmd) {
    parsed_args.command = Commands::UpgradeRemotes{upgrade_remotes};
  } else if (*add_package_cmd) {
    parsed_args.command = Commands::AddPackage{package_name, remote};
  } else {
    throw std::runtime_error("No command specified");
  }

  return parsed_args;
}

} // namespace App
