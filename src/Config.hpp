#pragma once

#include "Core.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace App {
constexpr inline std::string_view APPNAME = "apri";

namespace fs = std::filesystem;

fs::path get_default_config();
fs::path get_cache_dir();

namespace paths {
extern fs::path config;
extern fs::path cache;
} // namespace paths

struct Config {
  std::vector<Remote> remotes;
};

Config load_config(const fs::path &config_path);

} // namespace App
