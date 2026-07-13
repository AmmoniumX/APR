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
    return std::runtime_error(std::format("ERROR: {}. {} ({})", message,
                                          error->message, error->klass));
  } else {
    return std::runtime_error(message);
  }
}

template <typename T, void (*FreeFunc)(T *)> class GitHandle {
  using Handle = std::unique_ptr<T, decltype(FreeFunc)>;
  Handle handle_{nullptr, FreeFunc};

public:
  explicit GitHandle(T *handle) : handle_(handle, FreeFunc) {}

  // Virtual so derived wrapper types (Commit, Repository, ...) can be safely
  // destroyed through a GitHandle* base pointer.
  virtual ~GitHandle() = default;

  T *get() const { return handle_.get(); }
  T &operator*() const { return *handle_; }
  T *operator->() const { return handle_.get(); }
  explicit operator bool() const { return handle_ != nullptr; }
};

class Commit : public GitHandle<git_commit, git_commit_free> {
public:
  using GitHandle::GitHandle;
};

class Repository : public GitHandle<git_repository, git_repository_free> {
  std::filesystem::path path_;

public:
  explicit Repository(const std::filesystem::path &path)
      : GitHandle(open(path)), path_(path) {}

  bool fetch(const char *remote_name = "origin") {
    git_remote *remote_raw = nullptr;
    if (git_remote_lookup(&remote_raw, get(), remote_name) != 0) {
      throw make_error(
          std::format("Failed to lookup remote '{}'", remote_name));
    }
    GitHandle<git_remote, git_remote_free> remote(remote_raw);

    bool updated = false;
    git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
    opts.callbacks.payload = &updated;
    opts.callbacks.update_tips = [](const char *, const git_oid *a,
                                    const git_oid *b, void *payload) -> int {
      if (!git_oid_equal(a, b)) {
        *static_cast<bool *>(payload) = true;
      }
      return 0;
    };

    if (git_remote_fetch(remote.get(), nullptr, &opts, nullptr) != 0) {
      throw make_error("Failed to fetch from remote");
    }

    return updated;
  }

  bool pull(const char *remote_name = "origin") {
    fetch(remote_name);

    // Current branch, and the remote-tracking branch the fetch above just
    // updated (e.g. refs/heads/main -> refs/remotes/origin/main).
    git_reference *head_raw = nullptr;
    if (git_repository_head(&head_raw, get()) != 0) {
      throw make_error("Failed to get HEAD reference");
    }
    GitHandle<git_reference, git_reference_free> head_ref(head_raw);

    const char *branch_name = git_reference_shorthand(head_ref.get());
    std::string remote_ref_name =
        std::format("refs/remotes/{}/{}", remote_name, branch_name);

    git_reference *remote_ref_raw = nullptr;
    if (git_reference_lookup(&remote_ref_raw, get(),
                             remote_ref_name.c_str()) != 0) {
      throw make_error(
          std::format("Failed to lookup remote ref '{}'", remote_ref_name));
    }
    GitHandle<git_reference, git_reference_free> remote_ref(remote_ref_raw);

    const git_oid *remote_oid = git_reference_target(remote_ref.get());
    const git_oid *local_oid = git_reference_target(head_ref.get());

    if (git_oid_equal(remote_oid, local_oid)) {
      return false;
    }

    git_annotated_commit *annotated_raw = nullptr;
    if (git_annotated_commit_lookup(&annotated_raw, get(), remote_oid) != 0) {
      throw make_error("Failed to create annotated commit");
    }
    GitHandle<git_annotated_commit, git_annotated_commit_free> annotated(
        annotated_raw);

    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    const git_annotated_commit *annotated_arr[] = {annotated.get()};
    if (git_merge_analysis(&analysis, &preference, get(), annotated_arr,
                           1) != 0) {
      throw make_error("Failed to analyze merge");
    }

    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
      return false;
    }

    if (!(analysis & GIT_MERGE_ANALYSIS_FASTFORWARD)) {
      throw make_error(
          "Cannot fast-forward: local branch has diverged from remote");
    }

    git_object *target_raw = nullptr;
    if (git_object_lookup(&target_raw, get(), remote_oid,
                          GIT_OBJECT_COMMIT) != 0) {
      throw make_error("Failed to lookup target commit");
    }
    GitHandle<git_object, git_object_free> target(target_raw);

    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    if (git_checkout_tree(get(), target.get(), &checkout_opts) != 0) {
      throw make_error("Failed to checkout tree");
    }

    git_reference *new_ref_raw = nullptr;
    if (git_reference_set_target(&new_ref_raw, head_ref.get(), remote_oid,
                                 "pull: fast-forward") != 0) {
      throw make_error("Failed to update local branch reference");
    }
    GitHandle<git_reference, git_reference_free> new_ref(new_ref_raw);

    return true;
  }

  Commit head() const {
    git_oid oid;
    if (git_reference_name_to_id(&oid, get(), "HEAD") != 0) {
      throw make_error("Failed to get HEAD commit");
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, get(), &oid) != 0) {
      throw make_error("Failed to lookup commit");
    }

    return Commit(commit);
  }

  static Repository clone(const std::string &url,
                          const std::filesystem::path &path) {
    git_repository *repo = nullptr;
    if (git_clone(&repo, url.c_str(), path.c_str(), nullptr) != 0) {
      throw make_error("Failed to clone repository");
    }
    return Repository(repo, path);
  }

private:
  static git_repository *open(const std::filesystem::path &path) {
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, path.c_str()) != 0) {
      throw make_error("Failed to open repository", git_error_last());
    }
    return repo;
  }

  Repository(git_repository *repo, std::filesystem::path path)
      : GitHandle(repo), path_(std::move(path)) {}
};

} // namespace git2
