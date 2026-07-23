#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>

namespace App {
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

struct TransparentStringHash : std::hash<std::string_view> {
  using is_transparent = void;
};

using StringSet =
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>;

struct Remote {
  std::string name;
  std::string url;
  int priority = 1; // higher number means higher priority. Default is 1, 0 is
                    // the level for pacman.conf repositories.

  struct Whitelist {
    StringSet packages;
  };

  struct Blacklist {
    StringSet packages;
  };

  std::variant<Whitelist, Blacklist> packages =
      Blacklist{}; // Defaults to allow-all

  bool matches(std::string_view package) const {
    return std::visit(
        overloaded{[&package](const Whitelist &w) {
                     return w.packages.contains(package);
                   },
                   [&package](const Blacklist &b) {
                     return !b.packages.contains(package);
                   }},
        packages);
  }
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
