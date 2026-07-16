#pragma once

#include <filesystem>
#include <git2.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace git2 {

template <typename T, void (*FreeFunc)(T *)> class GitHandle {
  using Handle = std::unique_ptr<T, decltype(FreeFunc)>;
  Handle handle_{nullptr, FreeFunc};

public:
  explicit GitHandle(T *handle) : handle_(handle, FreeFunc) {}
  GitHandle(const GitHandle &) = delete;
  GitHandle(GitHandle &&other) noexcept : handle_(std::move(other.handle_)) {}

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

class Remote : public GitHandle<git_remote, git_remote_free> {
public:
  using GitHandle::GitHandle;
};

class Reference : public GitHandle<git_reference, git_reference_free> {
public:
  using GitHandle::GitHandle;
};

struct RemoteBranch {
  std::string name;
  std::string commit_hash;
};

class Repository : public GitHandle<git_repository, git_repository_free> {
  std::filesystem::path path_;

public:
  explicit Repository(const std::filesystem::path &path);

  Repository(const Repository &) = delete;
  Repository(Repository &&other) noexcept;

  std::filesystem::directory_entry root() const {
    return std::filesystem::directory_entry(path_);
  }

  bool fetch(const char *remote_name = "origin");
  bool pull(const char *remote_name = "origin");
  Commit head() const;
  Remote remote(const char *remote_name = "origin") const;
  std::vector<Reference> branches() const;

  static Repository clone(const char *url, const std::filesystem::path &path);
  static std::optional<Repository> try_open(const char *url,
                                            const std::filesystem::path &path,
                                            const char *remote_name = "origin");
  static std::vector<RemoteBranch> ls_remote_branches(const char *url);

private:
  static git_repository *open(const std::filesystem::path &path);

  Repository(git_repository *repo, std::filesystem::path path);
};

} // namespace git2
