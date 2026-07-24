#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace srcinfo {

// One `pkgbase` or `pkgname` section of a .SRCINFO file: its name and the
// `key = value` attribute lines that follow it, in file order. Multivalued
// keys (e.g. repeated `depends =` lines) and architecture-suffixed keys
// (e.g. `depends_x86_64`) are kept as literal keys; no override/merge
// semantics between the pkgbase section and a pkgname section are applied.
struct Section {
  std::string name;
  std::vector<std::pair<std::string, std::string>> fields;

  std::vector<std::string> values(std::string_view key) const;
  std::optional<std::string> value(std::string_view key) const;
};

struct SrcInfo {
  Section pkgbase;
  std::vector<Section> packages; // one per `pkgname` section

  static SrcInfo parse(std::string_view content);

  // "[epoch:]pkgver-pkgrel", in the same format pacman itself uses (and
  // what pacman::Package::Version::value expects).
  std::string version() const;
};

} // namespace srcinfo
