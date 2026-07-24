#pragma once

#include <expected>
#include <filesystem>
#include <span>
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
  SyncCommand command;
  bool verbose = false; // -v: enable debug-level logging
};

std::expected<Args, int> parse_args(std::span<char *> args);

} // namespace App
