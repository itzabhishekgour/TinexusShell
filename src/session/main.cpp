#include "common/logger.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <csignal>
#include <filesystem>
#include <thread>
#include <chrono>
#include <unordered_map>

using namespace tinexus;
namespace fs = std::filesystem;

struct ComponentState {
    std::string name;
    int restart_count{0};
    int backoff_ms{0};
    std::chrono::steady_clock::time_point last_crash;
};

static std::unordered_map<pid_t, ComponentState> g_managed_components;

static bool is_live_boot() {
    if (const char* env = std::getenv("TINEXUS_LIVE"); env && std::string_view(env) == "1") {
        return true;
    }
    if (fs::exists("/proc/cmdline")) {
        std::ifstream ifs("/proc/cmdline");
        std::string cmdline;
        if (std::getline(ifs, cmdline)) {
            if (cmdline.find("boot=live") != std::string::npos ||
                cmdline.find("rd.live.image") != std::string::npos ||
                cmdline.find("tinexus.live") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

static pid_t launch_component(const std::string& name) {
    pid_t pid = fork();
    if (pid == 0) {
        const char* runtime_dir_env = std::getenv("XDG_RUNTIME_DIR");
        std::string fallback_runtime = "/run/user/" + std::to_string(::getuid());
        const char* runtime_dir = (runtime_dir_env && *runtime_dir_env) ? runtime_dir_env : fallback_runtime.c_str();
        const char* wayland_disp_env = std::getenv("WAYLAND_DISPLAY");
        const char* wayland_disp = (wayland_disp_env && *wayland_disp_env) ? wayland_disp_env : "wayland-0";

        setenv("WAYLAND_DISPLAY", wayland_disp, 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
        setenv("QT_QPA_PLATFORM", "wayland", 1);
        setenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell", 1);

        // Propagate Qt6 Wayland platform plugin and QML search paths
        setenv("QT_PLUGIN_PATH", "/usr/lib/x86_64-linux-gnu/qt6/plugins", 0);
        setenv("QML_IMPORT_PATH", "/usr/lib/x86_64-linux-gnu/qt6/qml:/usr/share/tinexus", 0);
        setenv("QML2_IMPORT_PATH", "/usr/lib/x86_64-linux-gnu/qt6/qml:/usr/share/tinexus", 0);

        std::vector<std::string> search_paths = {
            "/usr/bin/tinexus-" + name,
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/tinexus/build/debug/src/" + name + "/tinexus-" + name
        };
        
        for (const auto& path : search_paths) {
            if (fs::exists(path)) {
                if (name == "launcher") {
                    execl(path.c_str(), ("tinexus-" + name).c_str(), "--daemon", nullptr);
                } else {
                    execl(path.c_str(), ("tinexus-" + name).c_str(), nullptr);
                }
            }
        }
        
        // Fallback to PATH
        if (name == "launcher") {
            execlp(("tinexus-" + name).c_str(), ("tinexus-" + name).c_str(), "--daemon", nullptr);
        } else {
            execlp(("tinexus-" + name).c_str(), ("tinexus-" + name).c_str(), nullptr);
        }
        _exit(127);
    }
    log::info("[Session] Spawned component 'tinexus-{}' (PID={})", name, pid);
    
    // Default initialize or keep existing state
    bool found = false;
    for (auto& [p, state] : g_managed_components) {
        if (state.name == name) {
            ComponentState st = state;
            g_managed_components.erase(p);
            g_managed_components[pid] = st;
            found = true;
            break;
        }
    }
    if (!found) {
        g_managed_components[pid] = {name, 0, 0, {}};
    }
    return pid;
}

static bool wait_for_socket_ready(const fs::path& socket_path, int timeout_ms = 15000) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (fs::exists(socket_path)) {
            int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd >= 0) {
                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
                if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                    ::close(fd);
                    return true;
                }
                ::close(fd);
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    log::set_component_name("session");
    log::info("[Session] Tinexus Desktop Session Supervisor starting...");

    const char* runtime_dir_env = std::getenv("XDG_RUNTIME_DIR");
    std::string fallback_runtime = "/run/user/" + std::to_string(::getuid());
    fs::path runtime_dir = (runtime_dir_env && *runtime_dir_env) ? runtime_dir_env : fallback_runtime;
    const char* wayland_disp_env = std::getenv("WAYLAND_DISPLAY");
    std::string wayland_disp = (wayland_disp_env && *wayland_disp_env) ? wayland_disp_env : "wayland-0";

    setenv("WAYLAND_DISPLAY", wayland_disp.c_str(), 1);
    setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);
    setenv("QT_QPA_PLATFORM", "wayland", 1);
    setenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell", 1);

    // 1. Wait for Wayland display socket to be created and connectable
    fs::path socket_path = runtime_dir / wayland_disp;
    log::info("[Session] Waiting for Wayland display socket at {}...", socket_path.string());

    if (wait_for_socket_ready(socket_path, 20000)) {
        log::info("[Session] Wayland display socket '{}' is ready!", socket_path.string());
    } else {
        log::error("[Session] Timeout waiting for Wayland socket '{}'!", socket_path.string());
    }

    // 2. Launch background infrastructure daemons
    fs::path session_bus_sock = runtime_dir / "bus";
    std::string dbus_addr = "unix:path=" + session_bus_sock.string();
    log::info("[Session] Starting D-Bus Session Bus...");
    pid_t dbus_pid = fork();
    if (dbus_pid == 0) {
        execl("/usr/bin/dbus-daemon", "dbus-daemon", "--session", "--nofork", "--nopidfile",
              ("--address=" + dbus_addr).c_str(), nullptr);
        _exit(127);
    }
    g_managed_components[dbus_pid] = {"dbus-session", 0, 0, {}};

    if (wait_for_socket_ready(session_bus_sock)) {
        log::info("[Session] D-Bus session socket is ready at {}", session_bus_sock.string());
    }
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr.c_str(), 1);

    // Launch IPC Daemon
    launch_component("ipcd");
    fs::path ipcd_sock = runtime_dir / "tinexus" / "ipc.sock";
    if (wait_for_socket_ready(ipcd_sock)) {
        log::info("[Session] IPC daemon socket ready at {}", ipcd_sock.string());
    }

    // Launch Search Daemon
    launch_component("searchd");

    // Launch Settings Daemon (io.tinexus.shell.Settings authoritative config daemon)
    launch_component("settings");

    // Launch Universal PipeWire / WirePlumber Audio Daemons (if present)
    if (fs::exists("/usr/bin/pipewire")) {
        fs::path pulse_dir = runtime_dir / "pulse";
        std::error_code ec;
        fs::create_directories(pulse_dir, ec);
        std::string pulse_server = "unix:" + (pulse_dir / "native").string();
        setenv("PULSE_SERVER", pulse_server.c_str(), 1);

        log::info("[Session] Starting PipeWire daemon (PULSE_SERVER={})...", pulse_server);
        pid_t pw_pid = fork();
        if (pw_pid == 0) {
            execl("/usr/bin/pipewire", "pipewire", nullptr);
            _exit(127);
        }
        if (pw_pid > 0) {
            g_managed_components[pw_pid] = {"pipewire", 0, 0, {}};
        }

        if (fs::exists("/usr/bin/pipewire-pulse")) {
            log::info("[Session] Starting PipeWire-Pulse compatibility daemon...");
            pid_t pwp_pid = fork();
            if (pwp_pid == 0) {
                execl("/usr/bin/pipewire-pulse", "pipewire-pulse", nullptr);
                _exit(127);
            }
            if (pwp_pid > 0) {
                g_managed_components[pwp_pid] = {"pipewire-pulse", 0, 0, {}};
            }
        }

        if (fs::exists("/usr/bin/wireplumber")) {
            log::info("[Session] Starting WirePlumber session manager...");
            pid_t wp_pid = fork();
            if (wp_pid == 0) {
                execl("/usr/bin/wireplumber", "wireplumber", nullptr);
                _exit(127);
            }
            if (wp_pid > 0) {
                g_managed_components[wp_pid] = {"wireplumber", 0, 0, {}};
            }
        }
    }

    // 3. Present lock screen on startup (with 3-attempt exponential backoff retry loop)
    log::info("[Session] Presenting tinexus-lock...");
    pid_t lock_pid = launch_component("lock");

    // Bounded gating on tinexus-lock with exponential backoff & retry limit
    constexpr int kMaxLockRetries = 3;
    int lock_retries = 0;
    int backoff_ms = 500;

    while (lock_retries < kMaxLockRetries) {
            int status = 0;
            pid_t p = waitpid(lock_pid, &status, 0);
            if (p == lock_pid) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    log::info("[Session] Component 'lock' exited cleanly (session unlocked). Not restarting.");
                    g_managed_components.erase(lock_pid);
                    break;
                } else {
                    lock_retries++;
                    g_managed_components.erase(lock_pid);

                    if (WIFSIGNALED(status)) {
                        log::warn("[Session] Component 'lock' killed by signal {} (attempt {}/{})",
                                  WTERMSIG(status), lock_retries, kMaxLockRetries);
                    } else {
                        log::warn("[Session] Component 'lock' exited abnormally with code {} (attempt {}/{})",
                                  WEXITSTATUS(status), lock_retries, kMaxLockRetries);
                    }

                    if (lock_retries >= kMaxLockRetries) {
                        log::error("[Session] CRITICAL: tinexus-lock failed {} consecutive times. "
                                   "Bypassing lock screen to prevent session lockout. Falling back to unlocked desktop.",
                                   lock_retries);
                        try {
                            fs::create_directories(runtime_dir / "tinexus");
                            std::ofstream warn_file(runtime_dir / "tinexus" / "lock_failure.warning");
                            warn_file << "Lock screen failed to start after 3 attempts. Session started unlocked.\n";
                        } catch (...) {}
                        break;
                    }

                    log::info("[Session] Waiting {} ms before respawning lock (attempt {}/{})...",
                              backoff_ms, lock_retries + 1, kMaxLockRetries);
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                    backoff_ms *= 2;
                    lock_pid = launch_component("lock");
                }
            } else if (p == -1 && errno == ECHILD) {
                log::warn("[Session] Lock process already reaped or missing.");
                break;
            }
        }

    // 4. Spawn desktop UI components without delay
    log::info("[Session] Spawning desktop components...");
    launch_component("wallpaper");
    launch_component("dock");
    launch_component("shell");
    launch_component("notifications");
    launch_component("launcher");

    log::info("[Session] All desktop shell components launched.");


    // Supervision loop: reap zombies & monitor daemons
    while (true) {
        int status = 0;
        pid_t p = waitpid(-1, &status, 0);
        if (p > 0) {
            log::warn("[Session] A shell component process (PID={}) exited with status {}.", p, WEXITSTATUS(status));
            
            auto it = g_managed_components.find(p);
            if (it != g_managed_components.end()) {
                ComponentState state = it->second;
                g_managed_components.erase(it);
                
                auto now = std::chrono::steady_clock::now();
                if (state.restart_count > 0 && std::chrono::duration_cast<std::chrono::seconds>(now - state.last_crash).count() > 30) {
                    // Stable for 30 seconds -> reset backoff
                    state.restart_count = 0;
                    state.backoff_ms = 0;
                }
                
                state.restart_count++;
                state.last_crash = now;
                
                if (state.name == "lock" && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    log::info("[Session] Component 'lock' exited cleanly (session unlocked). Not restarting.");
                } else if (state.name == "lock" && state.restart_count >= 3) {
                    log::error("[Session] Component 'lock' crashed {} times rapidly. ABORTING RESTART to prevent session lockout.", state.restart_count);
                } else if (state.restart_count > 5) {
                    log::error("[Session] Component '{}' crashed {} times rapidly. ABORTING RESTART to prevent crash loop.", state.name, state.restart_count);
                } else {
                    state.backoff_ms = state.restart_count == 1 ? 500 : state.backoff_ms * 2;
                    if (state.backoff_ms > 8000) state.backoff_ms = 8000;
                    
                    log::info("[Session] Restarting component '{}' in {} ms (attempt {})...", state.name, state.backoff_ms, state.restart_count);
                    std::this_thread::sleep_for(std::chrono::milliseconds(state.backoff_ms));
                    
                    pid_t new_pid = launch_component(state.name);
                    g_managed_components[new_pid].restart_count = state.restart_count;
                    g_managed_components[new_pid].backoff_ms = state.backoff_ms;
                    g_managed_components[new_pid].last_crash = state.last_crash;
                }
            }
        } else if (p == -1) {
            if (errno == ECHILD) {
                // All supervised children have exited — nothing left to watch.
                log::info("[Session] All shell components have exited. Session supervisor terminating cleanly.");
                break;
            }
            // Unexpected waitpid error (e.g. EINTR from a signal we did not handle).
            // Log and exit rather than spinning.
            log::error("[Session] waitpid() returned unexpected error (errno={}). Terminating.", errno);
            break;
        }
    }

    return 0;
}
