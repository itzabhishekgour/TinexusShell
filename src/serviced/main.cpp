#include "common/logger.hpp"
#include "common/version.hpp"
#include "serviced/daemon_spec.hpp"
#include "serviced/dep_graph.hpp"
#include "serviced/process_manager.hpp"
#include "serviced/runtime_socket.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <filesystem>

namespace {
tinexus::serviced::RuntimeControlSocket* g_socket{nullptr};
tinexus::serviced::ProcessManager* g_pm{nullptr};

void run_udev_setup() {
    auto run_cmd = [](const char* path, std::vector<char*> args) {
        if (!std::filesystem::exists(path)) return;
        pid_t pid = fork();
        if (pid == 0) {
            execv(path, args.data());
            _exit(127);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
        }
    };

    tinexus::log::info("Starting udev daemon for Wayland input device discovery...");
    run_cmd("/lib/systemd/systemd-udevd", {
        const_cast<char*>("/lib/systemd/systemd-udevd"),
        const_cast<char*>("--daemon"),
        nullptr
    });
    sleep(1); // Give udevd time to bind netlink and control sockets

    tinexus::log::info("Triggering udev device enumeration...");
    run_cmd("/usr/bin/udevadm", {
        const_cast<char*>("/usr/bin/udevadm"),
        const_cast<char*>("trigger"),
        const_cast<char*>("--action=add"),
        nullptr
    });

    run_cmd("/usr/bin/udevadm", {
        const_cast<char*>("/usr/bin/udevadm"),
        const_cast<char*>("settle"),
        const_cast<char*>("--timeout=5"),
        nullptr
    });
    sleep(1); // Ensure udev database is flushed to /run/udev/data/ before libinput starts

    tinexus::log::info("--- CHECKING /dev/input NODES ---");
    if (std::filesystem::exists("/dev/input")) {
        for (const auto& entry : std::filesystem::directory_iterator("/dev/input")) {
            tinexus::log::info("Found input node: {}", entry.path().string());
            run_cmd("/usr/bin/udevadm", {
                const_cast<char*>("/usr/bin/udevadm"),
                const_cast<char*>("info"),
                const_cast<char*>("--query=property"),
                const_cast<char*>("--name"),
                const_cast<char*>(entry.path().string().c_str()),
                nullptr
            });
        }
    } else {
        tinexus::log::error("/dev/input directory does NOT exist!");
    }
}

void signal_handler(int signal) {
    if (signal == SIGCHLD) {
        int status = 0;
        pid_t pid = 0;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (g_pm) {
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                int term_sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
                g_pm->handle_child_exit(pid, exit_code, term_sig);
            }
        }
    } else if (g_socket) {
        tinexus::log::info("Received signal {}, stopping tinexus-serviced...", signal);
        g_socket->stop();
    }
}
} // namespace

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-serviced");
    tinexus::log::info("Starting Platform Runtime Manager v{} (PID 1 Service Authority)...", tinexus::VERSION_STRING);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGCHLD, signal_handler);

    // Basic Environment Setup for PID 1 Session
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
    setenv("SHELL", "/bin/sh", 1);
    setenv("PATH", "/usr/bin:/usr/sbin:/bin:/sbin", 1);
    setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);
    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/0/bus", 1);

    // Wlroots compositor environment
    // WLR_DRM_NO_ATOMIC: Disable DRM atomic commits — virtio-gpu (QEMU) does not
    // support non-blocking atomic commits reliably, causing "Device or resource busy" errors.
    // Legacy commit path (setcrtc/setplane) is stable in QEMU.
    setenv("WLR_DRM_NO_ATOMIC", "1", 1);
    setenv("WLR_NO_HARDWARE_CURSORS", "1", 1);
    setenv("WLR_RENDERER", "pixman", 1);
    // Use 'builtin' standalone libseat backend for root compositors (opens physical DRM & input devices)
    setenv("LIBSEAT_BACKEND", "builtin", 1);
    setenv("WLR_LOG_LEVEL", "DEBUG", 1);

    // Ensure XDG_RUNTIME_DIR exists with correct permissions (0700)
    try {
        std::filesystem::create_directories("/run/user/0");
        chmod("/run/user/0", 0700);
    } catch (const std::exception& e) {
        tinexus::log::error("Failed to create /run/user/0: {}", e.what());
    }

    // Default Platform Supervision Graph
    tinexus::serviced::DependencyGraph graph;

    // Level 1: Central IPC Broker
    tinexus::serviced::DaemonSpec ipcd;
    ipcd.id = "ipcd";
    ipcd.executable = "tinexus-ipcd";
    ipcd.critical = true;
    graph.add_service(ipcd);

    // Level 2: Core Platform Services
    tinexus::serviced::DaemonSpec indexerd;
    indexerd.id = "indexerd";
    indexerd.executable = "tinexus-indexerd";
    indexerd.hard_dependencies = {"ipcd"};
    indexerd.critical = true;
    graph.add_service(indexerd);

    tinexus::serviced::DaemonSpec searchd;
    searchd.id = "searchd";
    searchd.executable = "tinexus-searchd";
    searchd.hard_dependencies = {"ipcd"};
    searchd.critical = true;
    graph.add_service(searchd);

    // Level 3: Wayland Compositor
    tinexus::serviced::DaemonSpec comp;
    comp.id = "comp";
    comp.executable = "tinexus-comp";
    comp.hard_dependencies = {"ipcd"};
    comp.critical = true;
    graph.add_service(comp);

    // Level 4: UI Surfaces
    tinexus::serviced::DaemonSpec shell;
    shell.id = "shell";
    shell.executable = "tinexus-shell";
    shell.hard_dependencies = {"comp", "searchd"};
    shell.critical = false;
    graph.add_service(shell);

    if (graph.has_cycle()) {
        tinexus::log::error("FATAL: Circular dependency detected in supervision tree!");
        return 1;
    }

    tinexus::serviced::ProcessManager pm(graph);
    g_pm = &pm;

    tinexus::serviced::RuntimeControlSocket socket(pm);
    g_socket = &socket;

    if (!socket.start()) {
        tinexus::log::error("Failed to start Runtime Control Socket");
        return 1;
    }

    tinexus::log::info("Platform Runtime Manager ready. Auto-spawning supervision tree...");
    run_udev_setup();
    pm.start_all_services();

    socket.run_accept_loop();
    return 0;
}
