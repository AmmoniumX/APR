#include <Args.hpp>
#include <Config.hpp>
#include <Core.hpp>
#include <git2_lib.hpp>
#include <print>
#include <span>

int main(int arc, char **argv) {
  auto args = std::span(argv, arc);

  auto parsed_args = App::parse_args(args);
  if (!parsed_args) {
    return parsed_args.error();
  }

  git_libgit2_init();

  auto config = App::load_config(parsed_args->config_path);
  std::println("Loaded {} remote(s) from {}", config.remotes.size(),
               parsed_args->config_path.string());

  git_libgit2_shutdown();
  return 0;
}
