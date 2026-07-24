#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
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

template <typename T>
using StringMap =
    std::unordered_map<std::string, T, TransparentStringHash, std::equal_to<>>;

struct Remote {
  static constexpr int PACMAN_REPOS_PRIORITY = 0;

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

  bool matches(std::string_view package) const;

  bool operator==(const Remote &other) const { return name == other.name; }
};

struct Package {
  std::string remote;
  std::string name;

  static Package parse(std::string_view unified);

  std::string unified() const;
};

// Throws if the calling process is running with root privileges.
void ensure_not_root();

} // namespace App

template <> struct std::hash<App::Remote> {
  std::size_t operator()(const App::Remote &r) const noexcept {
    return std::hash<std::string>{}(r.name);
  }
};
