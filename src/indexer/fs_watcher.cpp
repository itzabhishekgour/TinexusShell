#include "indexer/fs_watcher.hpp"
#include "common/logger.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>

namespace tinexus::indexer {

FsWatcher::~FsWatcher() {
    stop_watching();
}

bool FsWatcher::add_watch_directory(const std::filesystem::path& dir_path) {
    if (!std::filesystem::exists(dir_path)) {
        return false;
    }

    if (m_inotify_fd < 0) {
        m_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (m_inotify_fd < 0) {
            log::error("Failed to initialize inotify");
            return false;
        }
    }

    uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM;
    int wd = inotify_add_watch(m_inotify_fd, dir_path.c_str(), mask);
    if (wd < 0) {
        log::warn("Could not watch directory: {}", dir_path.string());
        return false;
    }

    m_watch_fds.push_back(wd);
    log::info("Watching application directory: {}", dir_path.string());
    return true;
}

void FsWatcher::start_watching(FileChangeCallback callback) {
    if (m_inotify_fd < 0 || m_watch_fds.empty()) {
        return;
    }

    m_running = true;
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    struct pollfd pfd;
    pfd.fd = m_inotify_fd;
    pfd.events = POLLIN;

    while (m_running) {
        int poll_res = poll(&pfd, 1, 500); // 500ms timeout check
        if (poll_res <= 0) {
            continue;
        }

        ssize_t len = read(m_inotify_fd, buffer, sizeof(buffer));
        if (len <= 0) {
            continue;
        }

        const struct inotify_event* event;
        for (char* ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
            event = reinterpret_cast<const struct inotify_event*>(ptr);
            if (event->len > 0) {
                std::filesystem::path changed_file(event->name);
                if (changed_file.extension() == ".desktop") {
                    FileChangeType change_type = FileChangeType::Modified;
                    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                        change_type = FileChangeType::Created;
                    } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                        change_type = FileChangeType::Deleted;
                    }
                    callback(changed_file, change_type);
                }
            }
        }
    }
}

void FsWatcher::stop_watching() {
    m_running = false;
    if (m_inotify_fd >= 0) {
        for (int wd : m_watch_fds) {
            inotify_rm_watch(m_inotify_fd, wd);
        }
        m_watch_fds.clear();
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }
}

} // namespace tinexus::indexer
