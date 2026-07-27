#include "terminal/pty_process.hpp"
#include "common/logger.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <termios.h>

namespace tinexus::terminal {

PtyProcess::~PtyProcess() {
    terminate();
}

bool PtyProcess::spawn(const std::string& shell_path, uint16_t cols, uint16_t rows) {
    log::info("PtyProcess: Opening master PTY...");
    m_master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (m_master_fd == -1) {
        log::error("PtyProcess: Failed to open master PTY!");
        return false;
    }

    if (grantpt(m_master_fd) != 0 || unlockpt(m_master_fd) != 0) {
        log::error("PtyProcess: Failed to grant/unlock PTY!");
        close(m_master_fd);
        m_master_fd = -1;
        return false;
    }

    char* pts_name = ptsname(m_master_fd);
    if (!pts_name) {
        log::error("PtyProcess: Failed to get pts name!");
        close(m_master_fd);
        m_master_fd = -1;
        return false;
    }

    set_window_size(cols, rows);

    m_child_pid = fork();
    if (m_child_pid < 0) {
        log::error("PtyProcess: Failed to fork child process!");
        close(m_master_fd);
        m_master_fd = -1;
        return false;
    }

    if (m_child_pid == 0) {
        // Child Process
        close(m_master_fd);
        setsid();

        int slave_fd = open(pts_name, O_RDWR);
        if (slave_fd == -1) {
            _exit(1);
        }

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > STDERR_FILENO) close(slave_fd);

        setenv("TERM", "xterm-256color", 1);
        const char* args[] = {shell_path.c_str(), nullptr};
        execvp(shell_path.c_str(), const_cast<char* const*>(args));
        _exit(1);
    }

    log::info("PtyProcess: Spawned child shell '{}' (PID: {}) on PTY '{}'", shell_path, m_child_pid, pts_name);
    return true;
}

ssize_t PtyProcess::read_bytes(char* buffer, size_t max_len) {
    if (m_master_fd < 0) return -1;
    return read(m_master_fd, buffer, max_len);
}

ssize_t PtyProcess::write_bytes(const char* data, size_t len) {
    if (m_master_fd < 0) return -1;
    return write(m_master_fd, data, len);
}

bool PtyProcess::set_window_size(uint16_t cols, uint16_t rows) {
    if (m_master_fd < 0) return false;
    struct winsize ws;
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    return ioctl(m_master_fd, TIOCSWINSZ, &ws) == 0;
}

bool PtyProcess::is_running() const noexcept {
    if (m_child_pid <= 0) return false;
    int status;
    pid_t res = waitpid(m_child_pid, &status, WNOHANG);
    return res == 0;
}

void PtyProcess::terminate() {
    if (m_child_pid > 0) {
        kill(m_child_pid, SIGTERM);
        waitpid(m_child_pid, nullptr, WNOHANG);
        m_child_pid = -1;
    }
    if (m_master_fd >= 0) {
        close(m_master_fd);
        m_master_fd = -1;
    }
}

} // namespace tinexus::terminal
