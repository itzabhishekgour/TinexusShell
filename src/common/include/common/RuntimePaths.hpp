#pragma once

#include <string>
#include <string_view>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <filesystem>

namespace tinexus::common {

class RuntimePaths {
public:
    /**
     * @brief Get the user runtime directory ($XDG_RUNTIME_DIR or /run/user/<uid>).
     */
    static inline std::string get_user_runtime_dir() noexcept {
        const char* xdg = ::getenv("XDG_RUNTIME_DIR");
        if (xdg && *xdg != '\0') {
            return std::string(xdg);
        }
        return "/run/user/" + std::to_string(::getuid());
    }

    /**
     * @brief Get the base Tinexus runtime directory ($XDG_RUNTIME_DIR/tinexus or /run/user/<uid>/tinexus).
     */
    static inline std::string get_runtime_dir() noexcept {
        return get_user_runtime_dir() + "/tinexus";
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
     * @brief Ensures that the Tinexus runtime directory exists with safe permissions.
     */
    static inline bool ensure_runtime_dir() noexcept {
        std::string dir = get_runtime_dir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!ec) {
            std::filesystem::permissions(dir,
                std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace, ec);
            return true;
        }
        return false;
    }
};

} // namespace tinexus::common
