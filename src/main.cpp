#include <Config.hpp>
#include <Core.hpp>
#include <git2_lib.hpp>
#include <print>
#include <span>

int main(int arc, char **argv) {
  auto args = std::span(argv, arc);

  git_libgit2_init();

  auto config = App::load_config(App::get_default_config_path());
  std::print("Loaded config with {} remotes and {} ignored packages\n",
             config.remotes.size(), config.ignored_packages.size());

  git_libgit2_shutdown();
  return 0;
}
