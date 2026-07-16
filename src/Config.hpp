#pragma once

#include "Core.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace App {
constexpr inline const char *APPNAME = "apra";

namespace fs = std::filesystem;

inline fs::path get_default_config() {
  auto user_config_dir = []() -> fs::path {
    if (const char *xdg_config_home = std::getenv("XDG_CONFIG_HOME");
        xdg_config_home) {
      return fs::path(xdg_config_home);
    } else if (const char *home = std::getenv("HOME"); home) {
      return fs::path(home) / ".config";
    } else {
      throw std::runtime_error("Could not determine default config path");
    }
  }();

  return user_config_dir / APPNAME / "config.yaml";
}

inline fs::path get_cache_dir() {
  auto user_cache_dir = []() -> fs::path {
    if (const char *xdg_cache_home = std::getenv("XDG_CACHE_HOME");
        xdg_cache_home) {
      return fs::path(xdg_cache_home);
    } else if (const char *home = std::getenv("HOME"); home) {
      return fs::path(home) / ".cache";
    } else {
      throw std::runtime_error("Could not determine default config path");
    }
  }();

  auto cache_dir = user_cache_dir / APPNAME;

  if (!fs::exists(cache_dir)) {
    fs::create_directory(cache_dir);
  }

  return cache_dir;
}

struct Config {
  std::vector<Remote> remotes;
  std::vector<Package> ignored_packages;
};

inline Config load_config(const fs::path &config_path = get_default_config()) {
  Config config;
  if (!fs::exists(config_path) || !fs::is_regular_file(config_path)) {
    return config;
  }

  YAML::Node config_yaml = YAML::LoadFile(config_path.string());
  for (const auto &remote : config_yaml["remotes"]) {
    Remote r{.name = remote["name"].as<std::string>(),
             .url = remote["url"].as<std::string>()};
    if (remote["priority"]) {
      r.priority = remote["priority"].as<int>();
    }

    // Check if the remote has a whitelist or blacklist
    if (remote["whitelist"]) {
      std::vector<std::string> whitelist;
      for (const auto &pkg : remote["whitelist"]) {
        whitelist.emplace_back(pkg.as<std::string>());
      }
      r.packages = Remote::Whitelist{std::move(whitelist)};
    } else if (remote["blacklist"]) {
      std::vector<std::string> blacklist;
      for (const auto &pkg : remote["blacklist"]) {
        blacklist.emplace_back(pkg.as<std::string>());
      }
      r.packages = Remote::Blacklist{std::move(blacklist)};
    }
    config.remotes.emplace_back(std::move(r));
  }

  for (const auto &ignored_package : config_yaml["ignored_packages"]) {
    config.ignored_packages.emplace_back(
        Package::parse(ignored_package.as<std::string>()));
  }

  return config;
}

} // namespace App
