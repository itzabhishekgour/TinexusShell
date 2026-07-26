#ifndef TINEXUS_INDEXER_FS_WATCHER_HPP
#define TINEXUS_INDEXER_FS_WATCHER_HPP

#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace tinexus::indexer {

enum class FileChangeType {
    Created,
    Modified,
    Deleted
};

using FileChangeCallback = std::function<void(const std::filesystem::path& path, FileChangeType change_type)>;

class FsWatcher {
public:
    FsWatcher() = default;
    ~FsWatcher();

    FsWatcher(const FsWatcher&) = delete;
    FsWatcher& operator=(const FsWatcher&) = delete;

    bool add_watch_directory(const std::filesystem::path& dir_path);
    void start_watching(FileChangeCallback callback);
    void stop_watching();

private:
    int m_inotify_fd{-1};
    std::vector<int> m_watch_fds;
    bool m_running{false};
};

} // namespace tinexus::indexer

#endif // TINEXUS_INDEXER_FS_WATCHER_HPP
