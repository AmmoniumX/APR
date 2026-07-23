#include <Core.hpp>

namespace App {

bool Remote::matches(std::string_view package) const {
  return std::visit(
      overloaded{[&package](const Whitelist &w) {
                   return w.packages.contains(package);
                 },
                 [&package](const Blacklist &b) {
                   return !b.packages.contains(package);
                 }},
      packages);
}

Package Package::parse(std::string_view unified) {
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

std::string Package::unified() const {
  if (remote.empty()) {
    return name;
  } else {
    return remote + "/" + name;
  }
}

} // namespace App
