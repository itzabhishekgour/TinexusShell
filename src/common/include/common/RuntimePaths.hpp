#pragma once

#include <string>
#include <string_view>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

namespace tinexus::common {

class RuntimePaths {
public:
    /**
     * @brief Get the base Tinexus runtime directory ($XDG_RUNTIME_DIR/tinexus or /run/user/<uid>/tinexus).
     */
    static inline std::string get_runtime_dir() noexcept {
        const char* xdg = ::getenv("XDG_RUNTIME_DIR");
        if (xdg && *xdg != '\0') {
            return std::string(xdg) + "/tinexus";
        }
        return "/run/user/" + std::to_string(::getuid()) + "/tinexus";
    }

    /**
     * @brief Path to the compositor command FIFO.
     */
    static inline std::string get_comp_fifo_path() noexcept {
        return get_runtime_dir() + "/comp-cmd.fifo";
    }

    /**
     * @brief Path to the tinexus-ipcd UNIX domain socket.
     */
    static inline std::string get_ipc_socket_path() noexcept {
        return get_runtime_dir() + "/ipc.sock";
    }

    /**
     * @brief Path to the single-instance flock lock file for a given app_id.
     */
    static inline std::string get_app_lock_path(std::string_view app_id) noexcept {
        return get_runtime_dir() + "/" + std::string(app_id) + ".lock";
    }

    /**
     * @brief Ensures that the Tinexus runtime directory exists with safe 0700 permissions.
     */
    static inline bool ensure_runtime_dir() noexcept {
        std::string dir = get_runtime_dir();
        struct stat st{};
        if (::stat(dir.c_str(), &st) != 0) {
            if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
                return false;
            }
        }
        return true;
    }
};

} // namespace tinexus::common
