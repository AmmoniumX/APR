#include <filesystem>
#include <git2.h>
#include <git2/types.h>
#include <memory>
#include <print>

namespace git2 {

class Repository {
  struct Deleter {
    void operator()(git_repository *repo) const { git_repository_free(repo); }
  };

  std::unique_ptr<git_repository, Deleter> repo_{nullptr};

public:
  Repository(const std::filesystem::path &path) {
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, path.c_str()) != 0) {
      throw std::runtime_error("Failed to open repository");
    }
    repo_.reset(repo);
  }

  git_repository *get() const { return repo_.get(); }

  git_commit *head() const {
    git_oid oid;
    if (git_reference_name_to_id(&oid, repo_.get(), "HEAD") != 0) {
      throw std::runtime_error("Failed to get HEAD commit");
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, repo_.get(), &oid) != 0) {
      throw std::runtime_error("Failed to lookup commit");
    }

    return commit;
  }
};
} // namespace git2

int main() {
  git_libgit2_init();

  std::println("Hello, World!");

  // Example repo usage
  git2::Repository repo(".");
  auto *commit = repo.head();
  std::println("HEAD commit ID: {}", git_oid_tostr_s(git_commit_id(commit)));
  std::print("Commit message: {}\n", git_commit_message(commit));

  git_libgit2_shutdown();
  return 0;
}
