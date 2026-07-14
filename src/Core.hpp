#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace App {

struct Remote {
  std::string name;
  std::string url;
  int priority = 1; // higher number means higher priority. Default is 1, 0 is
                    // the level for pacman.conf repositories.

  struct Whitelist {
    std::vector<std::string> packages;
  };

  struct Blacklist {
    std::vector<std::string> packages;
  };

  std::variant<Whitelist, Blacklist> packages =
      Blacklist{}; // Defaults to allow-all
};

struct Package {
  std::string remote;
  std::string name;

  static Package parse(std::string_view unified) {
    std::string remote{}, name{};
    auto pos = unified.find('/');
    if (pos == std::string_view::npos) {
      remote = "";
      name = std::string(unified);
    } else {
      remote = std::string(unified.substr(0, pos));
      name = std::string(unified.substr(pos + 1));
    }
    return Package{.remote = std::move(remote), .name = std::move(name)};
  }

  std::string unified() const {
    if (remote.empty()) {
      return name;
    } else {
      return remote + "/" + name;
    }
  }
};

} // namespace App
