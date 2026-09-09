#pragma once

#include <string>
#include <string_view>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>

namespace txui {

/**
 * @brief Tier-2 process-level Single-Instance RAII Guard.
 *
 * Uses kernel POSIX advisory flock(LOCK_EX | LOCK_NB) on a per-app lockfile
 * in the user's runtime directory ($XDG_RUNTIME_DIR/tinexus/<app_id>.lock).
 *
 * Guaranteed Properties:
 * 1. Race-safe against simultaneous launches (kernel serializes lock acquisition).
 * 2. Crash-resilient: When a process exits or crashes, the kernel automatically
 *    releases all file locks associated with its file descriptor table.
 * 3. No unlink race: Lock files are NOT deleted on exit, avoiding race conditions
 *    where a releasing instance unlinks a newly acquired lock file.
 * 4. Multi-instance applications (e.g. tinexus-terminal, tinexus-files) simply
 *    do not instantiate SingleInstance.
 */
class SingleInstance {
public:
    explicit SingleInstance(std::string_view app_id) noexcept
        : m_app_id(app_id) {
        if (m_app_id.empty()) {
            m_is_primary = true;
            return;
        }

        tinexus::common::RuntimePaths::ensure_runtime_dir();
        std::string lock_path = tinexus::common::RuntimePaths::get_app_lock_path(m_app_id);

        m_lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (m_lock_fd < 0) {
            tinexus::log::error("[SingleInstance] Failed to open lock file '{}': {}",
                                lock_path, strerror(errno));
            // In the rare event of filesystem error, allow launch to avoid blocking user
            m_is_primary = true;
            return;
        }

        int res = ::flock(m_lock_fd, LOCK_EX | LOCK_NB);
        if (res == 0) {
            // Successfully acquired exclusive lock -> Primary instance
            m_is_primary = true;
            tinexus::log::info("[SingleInstance] Acquired primary lock for '{}'", m_app_id);
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Lock held by active instance -> Secondary instance
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
            // Note: We deliberately do NOT unlink() the lockfile.
            // Closing the FD automatically releases the kernel flock atomically,
            // avoiding unlink races with concurrently launching instances.
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

    /**
     * @brief Returns true if this process acquired the exclusive primary lock.
     */
    [[nodiscard]] bool is_primary() const noexcept {
        return m_is_primary;
    }

    /**
     * @brief Sends a focus command to the compositor FIFO for any app_id.
     */
    static void focus_app(std::string_view app_id) noexcept {
        std::string fifo_path = tinexus::common::RuntimePaths::get_comp_fifo_path();
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

    /**
     * @brief Probes whether an application with the given app_id is currently running
     *        by attempting a non-blocking flock test on its lock file.
     *        If the lock is held (EWOULDBLOCK/EAGAIN), returns true.
     */
    static bool is_app_running(std::string_view app_id) noexcept {
        if (app_id.empty()) return false;
        std::string lock_path = tinexus::common::RuntimePaths::get_app_lock_path(app_id);
        int fd = ::open(lock_path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            return false;
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

    /**
     * @brief Sends a focus command to the compositor FIFO for this app_id.
     *        Used by secondary instances before exiting.
     */
    void request_focus_primary() const noexcept {
        focus_app(m_app_id);
    }

private:
    std::string m_app_id;
    int m_lock_fd{-1};
    bool m_is_primary{false};
};

} // namespace txui
