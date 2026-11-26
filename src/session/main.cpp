#include "common/logger.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
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

        // Hardware GPU acceleration configuration based on active compositor renderer
        std::string rend_path = "/run/tinexus/renderer";
        std::string active_renderer;
        if (fs::exists(rend_path)) {
            std::ifstream rf(rend_path);
            rf >> active_renderer;
        }
        if (active_renderer == "pixman") {
            setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
            setenv("QSG_RHI_BACKEND", "software", 1);
        } else {
            unsetenv("LIBGL_ALWAYS_SOFTWARE");
            setenv("QSG_RHI_BACKEND", "opengl", 1);
        }

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

static bool is_user_systemd_unit_active(const char* unit_name) {
    if (!unit_name || !fs::exists("/usr/bin/systemctl")) return false;
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::dup2(devnull, STDOUT_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            if (devnull > 2) ::close(devnull);
        }
        execl("/usr/bin/systemctl", "systemctl", "--user", "is-active", "--quiet", unit_name, nullptr);
        _exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool is_process_running_comm(const char* proc_name) {
    if (!proc_name) return false;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        if (!entry.is_directory(ec)) continue;
        std::string fname = entry.path().filename().string();
        if (fname.empty() || !std::isdigit(static_cast<unsigned char>(fname[0]))) continue;
        std::ifstream comm(entry.path() / "comm");
        std::string line;
        if (std::getline(comm, line) && line == proc_name) {
            return true;
        }
    }
    return false;
}

static bool wait_for_display_ready(const fs::path& runtime_dir, int timeout_ms = 5000) {
    auto start = std::chrono::steady_clock::now();
    const fs::path sys_display = "/run/tinexus/display";
    const fs::path user_display = runtime_dir / "tinexus" / "display";
    while (true) {
        std::error_code ec;
        if ((fs::exists(sys_display, ec) && fs::file_size(sys_display, ec) > 0) ||
            (fs::exists(user_display, ec) && fs::file_size(user_display, ec) > 0)) {
            return true;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

    // 1b. Wait for TinexusOutput mode/renderer initialization before mapping fullscreen lock overlay
    if (wait_for_display_ready(runtime_dir, 5000)) {
        log::info("[Session] Compositor output mode is ready (/run/tinexus/display).");
    } else {
        log::warn("[Session] Timeout waiting for /run/tinexus/display; proceeding with session startup.");
    }

    // 2. Launch background infrastructure daemons (respecting systemd --user sockets if already live)
    fs::path session_bus_sock = runtime_dir / "bus";
    std::string dbus_addr = "unix:path=" + session_bus_sock.string();
    if (wait_for_socket_ready(session_bus_sock, 300)) {
        log::info("[Session] D-Bus session socket already active at {}", session_bus_sock.string());
    } else {
        log::info("[Session] Starting D-Bus Session Bus...");
        pid_t dbus_pid = fork();
        if (dbus_pid == 0) {
            execl("/usr/bin/dbus-daemon", "dbus-daemon", "--session", "--nofork", "--nopidfile",
                  ("--address=" + dbus_addr).c_str(), nullptr);
            _exit(127);
        }
        if (dbus_pid > 0) {
            g_managed_components[dbus_pid] = {"dbus-session", 0, 0, {}};
        }
        if (wait_for_socket_ready(session_bus_sock, 5000)) {
            log::info("[Session] D-Bus session socket is ready at {}", session_bus_sock.string());
        }
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

    // Launch Universal PipeWire / WirePlumber Audio Daemons (if present and not managed by systemd --user)
    if (fs::exists("/usr/bin/pipewire")) {
        fs::path pw_sock = runtime_dir / "pipewire-0";
        fs::path pulse_dir = runtime_dir / "pulse";
        fs::path pulse_sock = pulse_dir / "native";
        std::error_code ec;
        fs::create_directories(pulse_dir, ec);
        std::string pulse_server = "unix:" + pulse_sock.string();
        setenv("PULSE_SERVER", pulse_server.c_str(), 1);

        const bool user_pw_active = is_user_systemd_unit_active("pipewire.service") ||
                                    is_user_systemd_unit_active("pipewire.socket") ||
                                    is_process_running_comm("pipewire");
        if (!wait_for_socket_ready(pw_sock, user_pw_active ? 2000 : 400) && !is_process_running_comm("pipewire")) {
            log::info("[Session] Starting PipeWire daemon (PULSE_SERVER={})...", pulse_server);
            pid_t pw_pid = fork();
            if (pw_pid == 0) {
                execl("/usr/bin/pipewire", "pipewire", nullptr);
                _exit(127);
            }
            if (pw_pid > 0) {
                g_managed_components[pw_pid] = {"pipewire", 0, 0, {}};
            }
            if (wait_for_socket_ready(pw_sock, 3000)) {
                log::info("[Session] PipeWire socket ready at {}", pw_sock.string());
            } else {
                log::warn("[Session] Timeout waiting for PipeWire socket at {}", pw_sock.string());
            }
        } else {
            log::info("[Session] PipeWire already active (systemd --user or process live).");
        }

        const bool user_pwp_active = is_user_systemd_unit_active("pipewire-pulse.service") ||
                                     is_user_systemd_unit_active("pipewire-pulse.socket") ||
                                     is_process_running_comm("pipewire-pulse");
        if (fs::exists("/usr/bin/pipewire-pulse") &&
            !wait_for_socket_ready(pulse_sock, user_pwp_active ? 2000 : 400) &&
            !is_process_running_comm("pipewire-pulse")) {
            log::info("[Session] Starting PipeWire-Pulse compatibility daemon...");
            pid_t pwp_pid = fork();
            if (pwp_pid == 0) {
                execl("/usr/bin/pipewire-pulse", "pipewire-pulse", nullptr);
                _exit(127);
            }
            if (pwp_pid > 0) {
                g_managed_components[pwp_pid] = {"pipewire-pulse", 0, 0, {}};
            }
            if (wait_for_socket_ready(pulse_sock, 3000)) {
                log::info("[Session] PipeWire-Pulse socket ready at {}", pulse_sock.string());
            }
        } else {
            log::info("[Session] PipeWire-Pulse already active.");
        }

        const bool wp_active = is_user_systemd_unit_active("wireplumber.service") ||
                               is_process_running_comm("wireplumber");
        if (fs::exists("/usr/bin/wireplumber") && !wp_active) {
            log::info("[Session] Starting WirePlumber session manager...");
            pid_t wp_pid = fork();
            if (wp_pid == 0) {
                execl("/usr/bin/wireplumber", "wireplumber", nullptr);
                _exit(127);
            }
            if (wp_pid > 0) {
                g_managed_components[wp_pid] = {"wireplumber", 0, 0, {}};
            }
        } else if (wp_active) {
            log::info("[Session] WirePlumber already active (systemd --user or process live).");
        }
    }

    // 3. Pre-warm hidden launcher daemon so Ctrl+K is ready immediately upon unlock
    launch_component("launcher");

    // 4. Present lock screen on startup
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

    // 5. Spawn desktop UI components without delay (top bar shell prioritized right after wallpaper)
    log::info("[Session] Spawning desktop components...");
    launch_component("wallpaper");
    launch_component("shell");
    launch_component("dock");
    launch_component("notifications");

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
                    
                    pid_t new_pid = -1;
                    if (state.name == "dbus-session") {
                        if (!wait_for_socket_ready(runtime_dir / "bus", 100)) {
                            new_pid = fork();
                            if (new_pid == 0) {
                                execl("/usr/bin/dbus-daemon", "dbus-daemon", "--session", "--nofork", "--nopidfile",
                                      ("--address=" + dbus_addr).c_str(), nullptr);
                                _exit(127);
                            }
                        }
                    } else if (state.name == "pipewire") {
                        if (!wait_for_socket_ready(runtime_dir / "pipewire-0", 150) && !is_process_running_comm("pipewire")) {
                            new_pid = fork();
                            if (new_pid == 0) {
                                execl("/usr/bin/pipewire", "pipewire", nullptr);
                                _exit(127);
                            }
                        }
                    } else if (state.name == "pipewire-pulse") {
                        if (!wait_for_socket_ready(runtime_dir / "pulse" / "native", 150) && !is_process_running_comm("pipewire-pulse")) {
                            new_pid = fork();
                            if (new_pid == 0) {
                                execl("/usr/bin/pipewire-pulse", "pipewire-pulse", nullptr);
                                _exit(127);
                            }
                        }
                    } else if (state.name == "wireplumber") {
                        if (!is_user_systemd_unit_active("wireplumber.service") && !is_process_running_comm("wireplumber")) {
                            new_pid = fork();
                            if (new_pid == 0) {
                                execl("/usr/bin/wireplumber", "wireplumber", nullptr);
                                _exit(127);
                            }
                        }
                    } else {
                        new_pid = launch_component(state.name);
                    }
                    if (new_pid > 0) {
                        g_managed_components[new_pid] = state;
                    }
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
