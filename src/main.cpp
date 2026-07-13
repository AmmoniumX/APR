#include <git2_lib.hpp>
#include <print>

int main() {
  git_libgit2_init();

  std::println("Hello, World!");

  // Example repo usage
  git2::Repository repo(".");
  repo.pull();
  auto *commit = repo.head();
  std::println("HEAD commit ID: {}", git_oid_tostr_s(git_commit_id(commit)));
  std::print("Commit message: {}\n", git_commit_message(commit));

  git_libgit2_shutdown();
  return 0;
}
