#ifndef TINEXUS_TERMINAL_PTY_PROCESS_HPP
#define TINEXUS_TERMINAL_PTY_PROCESS_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>

namespace tinexus::terminal {

class PtyProcess {
public:
    PtyProcess() = default;
    ~PtyProcess();

    // Spawns a child process and attaches it to a newly created PTY
    // Returns true on success, false otherwise
    bool spawn(const std::string& shell_path, const std::vector<std::string>& args_list = {}, uint16_t cols = 80, uint16_t rows = 24);
    ssize_t read_bytes(char* buffer, size_t max_len);
    ssize_t write_bytes(const char* data, size_t len);
    bool set_window_size(uint16_t cols, uint16_t rows);
    bool is_running() const noexcept;
    void terminate();

    [[nodiscard]] int master_fd() const noexcept { return m_master_fd; }

private:
    int m_master_fd{-1};
    pid_t m_child_pid{-1};
};

} // namespace tinexus::terminal

#endif // TINEXUS_TERMINAL_PTY_PROCESS_HPP
