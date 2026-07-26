#include "common/logger.hpp"
#include "common/version.hpp"
#include "serviced/daemon_spec.hpp"
#include "serviced/dep_graph.hpp"
#include "serviced/process_manager.hpp"
#include "serviced/runtime_socket.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include <iostream>
#include <csignal>
#include <sys/wait.h>

namespace {
tinexus::serviced::RuntimeControlSocket* g_socket{nullptr};
tinexus::serviced::ProcessManager* g_pm{nullptr};

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
    tinexus::serviced::DaemonSpec launcher;
    launcher.id = "launcher";
    launcher.executable = "tinexus-launcher";
    launcher.hard_dependencies = {"comp", "searchd"};
    launcher.critical = false;
    graph.add_service(launcher);

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
    pm.start_all_services();

    socket.run_accept_loop();
    return 0;
}
