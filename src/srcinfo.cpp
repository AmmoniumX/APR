#include <srcinfo.hpp>

#include <sstream>

namespace srcinfo {

std::vector<std::string> Section::values(std::string_view key) const {
  std::vector<std::string> result;
  for (const auto &[k, v] : fields) {
    if (k == key) {
      result.push_back(v);
    }
  }
  return result;
}

std::optional<std::string> Section::value(std::string_view key) const {
  for (const auto &[k, v] : fields) {
    if (k == key) {
      return v;
    }
  }
  return std::nullopt;
}

SrcInfo SrcInfo::parse(std::string_view content) {
  SrcInfo result;
  Section *current = &result.pkgbase;

  std::istringstream stream{std::string(content)};
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    std::string_view trimmed = line;
    while (!trimmed.empty() &&
           (trimmed.front() == ' ' || trimmed.front() == '\t')) {
      trimmed.remove_prefix(1);
    }
    if (trimmed.empty() || trimmed.front() == '#') {
      continue;
    }

    auto eq_pos = trimmed.find('=');
    if (eq_pos == std::string_view::npos) {
      continue;
    }

    std::string_view key = trimmed.substr(0, eq_pos);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
      key.remove_suffix(1);
    }

    std::string_view value = trimmed.substr(eq_pos + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
      value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
      value.remove_suffix(1);
    }

    if (key == "pkgbase") {
      result.pkgbase.name = std::string(value);
      current = &result.pkgbase;
    } else if (key == "pkgname") {
      result.packages.push_back(Section{.name = std::string(value)});
      current = &result.packages.back();
    } else {
      current->fields.emplace_back(std::string(key), std::string(value));
    }
  }

  return result;
}

std::string SrcInfo::version() const {
  auto epoch = pkgbase.value("epoch");
  auto pkgver = pkgbase.value("pkgver");
  auto pkgrel = pkgbase.value("pkgrel");

  std::string result;
  if (epoch && !epoch->empty() && *epoch != "0") {
    result += *epoch;
    result += ':';
  }
  result += pkgver.value_or("");
  if (pkgrel) {
    result += '-';
    result += *pkgrel;
  }
  return result;
}

} // namespace srcinfo
