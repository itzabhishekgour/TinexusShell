#include "common/logger.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <csignal>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace tinexus;
namespace fs = std::filesystem;

static pid_t launch_component(const std::string& name) {
    pid_t pid = fork();
    if (pid == 0) {
        setenv("WAYLAND_DISPLAY", "wayland-0", 1);
        setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);

        std::vector<std::string> search_paths = {
            "/usr/bin/tinexus-" + name,
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/tinexus/build/debug/src/" + name + "/tinexus-" + name
        };
        
        for (const auto& path : search_paths) {
            if (fs::exists(path)) {
                execl(path.c_str(), ("tinexus-" + name).c_str(), nullptr);
            }
        }
        
        // Fallback to PATH
        execlp(("tinexus-" + name).c_str(), ("tinexus-" + name).c_str(), nullptr);
        _exit(127);
    }
    log::info("[Session] Spawned component 'tinexus-{}' (PID={})", name, pid);
    return pid;
}

int main(int argc, char* argv[]) {
    log::set_component_name("session");
    log::info("[Session] Tinexus Desktop Session Supervisor starting...");

    // Wait for Wayland socket /run/user/0/wayland-0 to be created by tinexus-comp
    fs::path socket_path = "/run/user/0/wayland-0";
    log::info("[Session] Waiting for Wayland display socket at {}...", socket_path.string());

    for (int i = 0; i < 30; ++i) { // up to 15 seconds wait
        if (fs::exists(socket_path)) {
            log::info("[Session] Wayland display socket '{}' is ready!", socket_path.string());
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    setenv("WAYLAND_DISPLAY", "wayland-0", 1);
    setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);

    // Launch Desktop Shell Components in correct order:
    // 0. IPC Daemon (MUST be first — all other components depend on it)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    launch_component("ipcd");

    // Wait for ipcd socket to be ready before launching dependents
    fs::path ipcd_sock = "/run/user/0/tinexus/ipc.sock";
    for (int i = 0; i < 30; ++i) {
        if (fs::exists(ipcd_sock)) {
            log::info("[Session] IPC daemon socket ready at {}", ipcd_sock.string());
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 1. Background Wallpaper
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    launch_component("wallpaper");

    // 2. Unified Shell (Dynamic Island + Launcher)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    launch_component("shell");

    // 3. Notification Center Daemon
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    launch_component("notifications");

    // 4. Lock Screen (overlay on top of desktop)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    launch_component("lock");

    log::info("[Session] All desktop shell components launched.");


    // Supervision loop: reap zombies & monitor daemons
    while (true) {
        int status = 0;
        pid_t p = waitpid(-1, &status, 0);
        if (p > 0) {
            log::warn("[Session] A shell component process (PID={}) exited with status {}.", p, WEXITSTATUS(status));
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
