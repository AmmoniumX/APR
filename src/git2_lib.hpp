#include <filesystem>
#include <format>
#include <git2.h>
#include <git2/errors.h>
#include <memory>
#include <stdexcept>

namespace git2 {

inline std::runtime_error
make_error(const std::string &message,
           const git_error *error = git_error_last()) {
  if (error) {
    return std::runtime_error(std::format("{}: {}", message, error->message));
  } else {
    return std::runtime_error(message);
  }
}

class Repository {
  struct Deleter {
    void operator()(git_repository *repo) const { git_repository_free(repo); }
  };

  std::unique_ptr<git_repository, Deleter> repo_{nullptr};
  std::filesystem::path path_;

public:
  Repository(const std::filesystem::path &path) : path_(path) {
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, path.c_str()) != 0) {
      throw make_error("Failed to open repository", git_error_last());
    }
    repo_.reset(repo);
  }

  git_repository *get() const { return repo_.get(); }

  Repository &pull() {
    git_remote *remote = nullptr;
    if (git_remote_lookup(&remote, repo_.get(), "origin") != 0) {
      throw make_error("Failed to lookup remote 'origin'");
    }

    if (git_remote_fetch(remote, nullptr, nullptr, nullptr) != 0) {
      git_remote_free(remote);
      throw make_error("Failed to fetch from remote");
    }

    git_remote_free(remote);
    return *this;
  }

  git_commit *head() const {
    git_oid oid;
    if (git_reference_name_to_id(&oid, repo_.get(), "HEAD") != 0) {
      throw make_error("Failed to get HEAD commit");
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, repo_.get(), &oid) != 0) {
      throw make_error("Failed to lookup commit");
    }

    return commit;
  }
};

inline Repository clone(const std::string &url,
                        const std::filesystem::path &path) {
  git_repository *repo = nullptr;
  if (git_clone(&repo, url.c_str(), path.c_str(), nullptr) != 0) {
    throw make_error("Failed to clone repository");
  }
  return Repository(path);
}

} // namespace git2
