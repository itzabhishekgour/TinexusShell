#pragma once

#include <string>
#include <string_view>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <utility>
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <common/AppId.hpp>

namespace tinexus::common {

class SingleInstance {
public:
    explicit SingleInstance(std::string_view app_id) noexcept
        : m_app_id(app_id) {
        if (m_app_id.empty()) {
            m_is_primary = true;
            return;
        }

        RuntimePaths::ensure_runtime_dir();
        std::string lock_path = RuntimePaths::get_app_lock_path(m_app_id);

        m_lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (m_lock_fd < 0) {
            std::string tmp_dir = "/tmp/tinexus";
            std::error_code ec;
            std::filesystem::create_directories(tmp_dir, ec);
            std::string fallback_path = tmp_dir + "/" + std::string(m_app_id) + ".lock";
            m_lock_fd = ::open(fallback_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
            if (m_lock_fd < 0) {
                tinexus::log::error("[SingleInstance] Failed to open lock file '{}' and fallback '{}': {}",
                                    lock_path, fallback_path, strerror(errno));
                m_is_primary = true;
                return;
            }
            lock_path = fallback_path;
        }

        int res = ::flock(m_lock_fd, LOCK_EX | LOCK_NB);
        if (res == 0) {
            m_is_primary = true;
            tinexus::log::info("[SingleInstance] Acquired primary lock for '{}'", m_app_id);
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                m_is_primary = false;
                tinexus::log::info("[SingleInstance] App '{}' is already running; secondary instance detected", m_app_id);
            } else {
                tinexus::log::warn("[SingleInstance] flock failed with errno {}: {}", errno, strerror(errno));
                m_is_primary = true;
            }
        }
    }

    ~SingleInstance() noexcept {
        if (m_lock_fd >= 0) {
            ::close(m_lock_fd);
            m_lock_fd = -1;
        }
    }

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;
    SingleInstance(SingleInstance&& other) noexcept
        : m_app_id(std::move(other.m_app_id)),
          m_lock_fd(other.m_lock_fd),
          m_is_primary(other.m_is_primary) {
        other.m_lock_fd = -1;
        other.m_is_primary = false;
    }
    SingleInstance& operator=(SingleInstance&& other) noexcept {
        if (this != &other) {
            if (m_lock_fd >= 0) {
                ::close(m_lock_fd);
            }
            m_app_id = std::move(other.m_app_id);
            m_lock_fd = other.m_lock_fd;
            m_is_primary = other.m_is_primary;
            other.m_lock_fd = -1;
            other.m_is_primary = false;
        }
        return *this;
    }

    [[nodiscard]] bool is_primary() const noexcept {
        return m_is_primary;
    }

    static void focus_app(std::string_view app_id) noexcept {
        std::string fifo_path = RuntimePaths::get_comp_fifo_path();
        int fifo_fd = ::open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
        if (fifo_fd >= 0) {
            std::string cmd = "focus " + std::string(app_id) + "\n";
            ssize_t written = ::write(fifo_fd, cmd.data(), cmd.size());
            (void)written;
            ::close(fifo_fd);
            tinexus::log::info("[SingleInstance] Sent focus request for '{}' to compositor FIFO", app_id);
        } else {
            tinexus::log::warn("[SingleInstance] Failed to open compositor command FIFO '{}': {}",
                              fifo_path, strerror(errno));
        }
    }

    static bool is_app_running(std::string_view app_id) noexcept {
        if (app_id.empty()) return false;
        std::string lock_path = RuntimePaths::get_app_lock_path(app_id);
        int fd = ::open(lock_path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            std::string fallback_path = "/tmp/tinexus/" + std::string(app_id) + ".lock";
            fd = ::open(fallback_path.c_str(), O_RDWR | O_CLOEXEC);
            if (fd < 0) return false;
        }
        int res = ::flock(fd, LOCK_EX | LOCK_NB);
        if (res == 0) {
            ::flock(fd, LOCK_UN);
            ::close(fd);
            return false;
        }
        ::close(fd);
        return (errno == EWOULDBLOCK || errno == EAGAIN);
    }

    void request_focus_primary() const noexcept {
        focus_app(m_app_id);
    }

private:
    std::string m_app_id;
    int m_lock_fd{-1};
    bool m_is_primary{false};
};

} // namespace tinexus::common
