#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <pacman_lib.hpp>

#include <alpm.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/process.hpp>

#include <filesystem>
#include <format>
#include <sstream>
#include <stdexcept>

namespace pacman {

namespace asio = boost::asio;
namespace bp = boost::process;

std::strong_ordering Package::Version::operator<=>(const Version &other) const {
  int rc = alpm_pkg_vercmp(value.c_str(), other.value.c_str());
  if (rc < 0) {
    return std::strong_ordering::less;
  }
  if (rc > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

bool Package::Version::operator==(const Version &other) const {
  return (*this <=> other) == 0;
}

std::expected<std::vector<Package>, int> query_installed_packages() {
  auto exe = bp::environment::find_executable("pacman");
  if (exe.empty()) {
    throw std::runtime_error("pacman executable not found in PATH");
  }

  asio::io_context ctx;
  bp::popen proc(ctx, exe, {"-Q"});

  std::string output;
  boost::system::error_code ec;
  asio::read(proc, asio::dynamic_buffer(output), ec);
  if (ec && ec != asio::error::eof) {
    throw std::runtime_error(
        std::format("Failed to read pacman output: {}", ec.message()));
  }

  int exit_code = proc.wait();
  if (exit_code != 0) {
    return std::unexpected(exit_code);
  }

  std::vector<Package> packages;
  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    auto space_pos = line.find(' ');
    if (space_pos == std::string::npos) {
      continue;
    }

    packages.push_back(Package{
        .name = line.substr(0, space_pos),
        .version = Package::Version{.value = line.substr(space_pos + 1)},
    });
  }

  return packages;
}

namespace {

std::expected<void, int> run_sudo_pacman(std::vector<std::string> args) {
  auto exe = bp::environment::find_executable("sudo");
  if (exe.empty()) {
    throw std::runtime_error("sudo executable not found in PATH");
  }

  asio::io_context ctx;
  bp::process proc(ctx, exe, args);

  int exit_code = proc.wait();
  if (exit_code != 0) {
    return std::unexpected(exit_code);
  }
  return {};
}

} // namespace

std::expected<void, int>
run_makepkg_sync_install(const std::filesystem::path &directory) {
  auto exe = bp::environment::find_executable("makepkg");
  if (exe.empty()) {
    throw std::runtime_error("makepkg not found in PATH");
  }

  asio::io_context ctx;
  bp::process proc(ctx, exe, {"-si"}, bp::process_start_dir(directory.c_str()));

  int exit_code = proc.wait();
  if (exit_code != 0) {
    return std::unexpected(exit_code);
  }
  return {};
}

std::expected<void, int> refresh() {
  return run_sudo_pacman({"pacman", "-Sy"});
}

std::expected<void, int> install_packages(std::span<std::string> packages) {
  if (packages.empty()) {
    return {};
  }

  std::vector<std::string> args{"pacman", "-S"};
  args.insert(args.end(), packages.begin(), packages.end());
  return run_sudo_pacman(std::move(args));
}

std::expected<void, int>
upgrade_and_install_packages(std::span<std::string> packages) {
  std::vector<std::string> args{"pacman", "-Su"};
  args.insert(args.end(), packages.begin(), packages.end());
  return run_sudo_pacman(std::move(args));
}

} // namespace pacman
