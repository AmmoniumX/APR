#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace App {

struct Remote {
  std::string name;
  std::string url;

  struct Whitelist {
    std::vector<std::string> packages;
  };

  struct Blacklist {
    std::vector<std::string> packages;
  };

  std::variant<Whitelist, Blacklist> packages =
      Blacklist{}; // Defaults to allow-all

  explicit Remote(const std::string &name, const std::string &url)
      : name(name), url(url) {}

  explicit Remote(const std::string &name, const std::string &url,
                  Whitelist &&whitelist)
      : name(name), url(url), packages(std::move(whitelist)) {}

  explicit Remote(const std::string &name, const std::string &url,
                  Blacklist &&blacklist)
      : name(name), url(url), packages(std::move(blacklist)) {}
};

struct Package {
  std::string remote;
  std::string name;

  explicit Package(const std::string &remote, const std::string &name)
      : remote(remote), name(name) {}

  explicit Package(std::string_view unified) {
    auto pos = unified.find('/');
    if (pos == std::string_view::npos) {
      remote = "";
      name = std::string(unified);
    } else {
      remote = std::string(unified.substr(0, pos));
      name = std::string(unified.substr(pos + 1));
    }
  }
};

} // namespace App
