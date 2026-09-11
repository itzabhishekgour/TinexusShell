#include "serviced/launch_authority.hpp"
#include "serviced/event_journal.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <unordered_set>
#include "guard/crypto_validator.hpp"

extern char **environ;

namespace tinexus::serviced {

LaunchAuthority& LaunchAuthority::instance() noexcept {
    static LaunchAuthority s_instance;
    return s_instance;
}

static bool check_override_cache(const std::string& sha256_hex) {
    std::ifstream file("/var/lib/tinexus/trust-overrides.conf");
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line == sha256_hex) return true;
    }
    return false;
}

static std::vector<uint8_t> read_sig_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int LaunchAuthority::validate_and_get_fd(const std::string& exec_cmd) const {
    if (exec_cmd.empty()) return -1;
    if (exec_cmd.find(';') != std::string::npos || exec_cmd.find('&') != std::string::npos ||
        exec_cmd.find('|') != std::string::npos || exec_cmd.find('`') != std::string::npos) {
        return -1;
    }

    // Only apply cryptographic verification to 3rd party apps
    if (!exec_cmd.starts_with("/opt/tinexus-apps/")) {
        return -2; // Base system binary
    }

    int app_fd = open(exec_cmd.c_str(), O_RDONLY | O_CLOEXEC);
    if (app_fd < 0) return -1;

    // 1. Compute SHA256 of the FD to check against override cache
    std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(app_fd);
    if (check_override_cache(hash)) {
        log::info("LaunchAuthority: App {} verified via trust override cache.", exec_cmd);
        return app_fd;
    }

    // 2. Not overridden, check Ed25519 signature
    std::string sig_path = exec_cmd + ".sig";
    auto sig_bytes = read_sig_file(sig_path);
    if (sig_bytes.empty() || !tinexus::guard::CryptoValidator::verify_signature_fd(app_fd, sig_bytes, "/etc/tinexus/keys/root.pub")) {
        log::error("LaunchAuthority: Cryptographic signature verification failed for {}", exec_cmd);
        close(app_fd);
        return -1;
    }

    log::info("LaunchAuthority: App {} verified via Ed25519 signature.", exec_cmd);
    return app_fd;
}

pid_t LaunchAuthority::execute_action(const ActionRequest& req) {
    int app_fd = req.target_fd;
    log::info("LaunchAuthority: Executing ActionRequest Type {} -> Target: {} (FD: {})", static_cast<int>(req.type), req.target, app_fd);
    EventJournal::instance().log_event("action_authority", "ACTION_EXEC", req.target);

    if (req.type == ActionType::OpenFile) {
        if (!req.target.ends_with(".AppImage") && !req.target.ends_with(".txapp")) {
            log::error("LaunchAuthority: Rejected OpenFile for non-executable type: {}", req.target);
            return -1;
        }
        if (req.target.ends_with(".txapp")) {
            std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(app_fd);
            if (!check_override_cache(hash)) {
                std::string sig_path = req.target + ".sig";
                auto sig_bytes = read_sig_file(sig_path);
                if (sig_bytes.empty() || !tinexus::guard::CryptoValidator::verify_signature_fd(app_fd, sig_bytes, "/etc/tinexus/keys/root.pub")) {
                    log::error("LaunchAuthority: Cryptographic signature verification failed for OpenFile target {}", req.target);
                    return -1;
                }
            }
        }

    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();

        if (!req.working_dir.empty()) {
            chdir(req.working_dir.c_str());
        }

        // Drop privileges to the target user (usually UID 1000 for Wayland session)
        if (getuid() == 0) {
            uid_t target_uid = 1000;
            gid_t target_gid = 1000;
            
            // Try to lookup 'tinexus' or first non-root user
            struct passwd* pw = getpwnam("tinexus");
            if (pw) {
                target_uid = pw->pw_uid;
                target_gid = pw->pw_gid;
            }

            setgroups(0, nullptr);       // Clear supplementary groups
            setgid(target_gid);          // GID must be set first
            setuid(target_uid);          // UID set last
        }

        // Set minimal, sanitized environment
        std::vector<std::string> clean_env_strings = {
            "HOME=/home/tinexus",
            "USER=tinexus",
            "LOGNAME=tinexus",
            "PATH=/usr/bin:/bin:/usr/local/bin:/opt/tinexus-apps",
            "XDG_RUNTIME_DIR=/run/user/0",
            "WAYLAND_DISPLAY=wayland-0",
            "LANG=C.UTF-8",
            "LC_ALL=C.UTF-8",
            "QT_QPA_PLATFORM=wayland",
            "QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins",
            "QML2_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml",
            "QML_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml",
            "TINEXUS_SETTINGS_QML=/usr/share/tinexus-settings/qml/MainWindow.qml"
        };
        
        static const std::unordered_set<std::string> ALLOWED_OVERRIDE_KEYS = {
            "APP_LOCALE", "APP_THEME_MODE", "TZ"
        };
        for (const auto& [k, v] : req.env) {
            if (ALLOWED_OVERRIDE_KEYS.count(k)) {
                clean_env_strings.push_back(k + "=" + v);
            } else {
                log::warn("LaunchAuthority: Dropped non-allowlisted env var: {}", k);
            }
        }
        
        std::vector<char*> child_env;
        for (auto& s : clean_env_strings) {
            child_env.push_back(const_cast<char*>(s.c_str()));
        }
        child_env.push_back(nullptr);

        // Prepare execve arguments array cleanly without shell evaluation
        std::vector<char*> args;
        args.push_back(const_cast<char*>(req.target.c_str()));
        for (const auto& arg : req.arguments) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        // 1. Explicit FD Sanitization
#ifdef __linux__
#include <linux/close_range.h>
#include <sys/syscall.h>
        int sys_ret = -1;
        if (app_fd >= 3) {
            int r1 = (app_fd > 3) ? static_cast<int>(syscall(__NR_close_range, 3, app_fd - 1, 0)) : 0;
            int r2 = static_cast<int>(syscall(__NR_close_range, app_fd + 1, ~0U, 0));
            sys_ret = (r1 == 0 && r2 == 0) ? 0 : -1;
        } else {
            sys_ret = static_cast<int>(syscall(__NR_close_range, 3, ~0U, 0));
        }
        if (sys_ret != 0) {
#endif
            // Fallback for older kernels or if close_range fails
            DIR* dir = opendir("/proc/self/fd");
            if (dir != nullptr) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    int fd = atoi(entry->d_name);
                    if (fd > 2 && fd != dirfd(dir) && fd != app_fd) {
                        close(fd);
                    }
                }
                closedir(dir);
            }
#ifdef __linux__
        }
#endif

        // 2. Strict Privilege Drop Sequence
        constexpr uid_t TINEXUS_USER_UID = 1000;
        constexpr gid_t TINEXUS_USER_GID = 1000;

        // Clear root supplementary groups (Security Critical)
        if (setgroups(0, nullptr) != 0) {
            log::error("LaunchAuthority: FATAL: setgroups failed");
            _exit(1); 
        }
        // Drop GID
        if (setgid(TINEXUS_USER_GID) != 0) { 
            log::error("LaunchAuthority: FATAL: setgid failed");
            _exit(1); 
        }
        // Drop UID (Point of no return)
        if (setuid(TINEXUS_USER_UID) != 0) { 
            log::error("LaunchAuthority: FATAL: setuid failed");
            _exit(1); 
        }

        // If we have an FD (third-party app or AppImage), use it to prevent TOCTOU
        if (app_fd >= 0) {
            if (req.target.ends_with(".AppImage")) {
                // Remove O_CLOEXEC so the FD survives into tx-appimage
                int flags = fcntl(app_fd, F_GETFD);
                if (flags != -1) {
                    fcntl(app_fd, F_SETFD, flags & ~FD_CLOEXEC);
                }
                std::string fd_path = "/proc/self/fd/" + std::to_string(app_fd);
                execlp("tx-appimage", "tx-appimage", fd_path.c_str(), nullptr);
            } else {
                fexecve(app_fd, args.data(), child_env.data());
            }
        } else {
            execve(req.target.c_str(), args.data(), child_env.data());
        }
        _exit(127);
    }

    if (pid > 0) {
        log::info("LaunchAuthority: Action executed successfully with PID {}", pid);
    } else {
        log::error("LaunchAuthority: Failed to fork action process for target '{}'", req.target);
    }

    return pid;
}

pid_t LaunchAuthority::launch_app(const std::string& app_id, const std::string& exec_cmd) {
    int app_fd = validate_and_get_fd(exec_cmd);
    if (app_fd == -1) {
        log::error("LaunchAuthority: Rejected invalid/unsafe exec string for app '{}': {}", app_id, exec_cmd);
        EventJournal::instance().log_event(app_id, "LAUNCH_REJECTED", "Unsafe exec command");
        return -1;
    }

    ActionRequest req;
    req.type = ActionType::AppLaunch;
    req.target = exec_cmd;
    req.target_fd = app_fd;
    pid_t child_pid = execute_action(req);
    
    // Close the FD in the parent process, since it was passed to execute_action and duped/execed in the child
    if (app_fd >= 0) {
        close(app_fd);
    }
    
    return child_pid;
}

} // namespace tinexus::serviced
