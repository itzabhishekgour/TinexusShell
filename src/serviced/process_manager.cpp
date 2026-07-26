#include "serviced/process_manager.hpp"
#include "serviced/event_journal.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace tinexus::serviced {

namespace {
std::string resolve_binary_path(const std::string& name) {
    if (fs::exists(name)) return name;

    const char* home = std::getenv("HOME");
    if (home) {
        fs::path debug_dir = fs::path(home) / "tinexus" / "build" / "debug";
        std::vector<std::string> subdirs = {"src/ipcd", "src/indexer", "src/searchd", "src/comp", "src/launcher", "tools/tinexusctl", "tools/tinexus-comp-inspector"};
        for (const auto& sub : subdirs) {
            fs::path p = debug_dir / sub / name;
            if (fs::exists(p)) return p.string();
        }
    }

    fs::path usr_p = fs::path("/usr/bin") / name;
    if (fs::exists(usr_p)) return usr_p.string();

    return name;
}
} // namespace

ProcessManager::ProcessManager(DependencyGraph graph)
    : m_graph(std::move(graph)) {
    auto startup_order = m_graph.get_startup_order();
    for (const auto& id : startup_order) {
        auto spec = m_graph.get_spec(id);
        if (spec) {
            m_states[id] = ManagedServiceState{*spec, DaemonStatus::Stopped, -1, 0, 0};
        }
    }
}

ProcessManager::~ProcessManager() {
    stop_all_services();
}

uint32_t ProcessManager::calculate_backoff_ms(int attempt, bool is_critical) noexcept {
    if (attempt <= 1) return 0;       // Attempt 1: Immediate (0ms)
    if (attempt == 2) return 2000;    // Attempt 2: 2 seconds
    if (attempt == 3) return 8000;    // Attempt 3: 8 seconds
    if (attempt == 4) return 32000;   // Attempt 4: 32 seconds

    // Max backoff cap for critical services is 60s
    return 60000;
}

bool ProcessManager::start_service(const std::string& service_id) {
    auto it = m_states.find(service_id);
    if (it == m_states.end()) return false;

    auto& state = it->second;
    if (state.status == DaemonStatus::Running || state.status == DaemonStatus::Starting) {
        return true;
    }

    // Check hard dependencies first
    for (const auto& dep : state.spec.hard_dependencies) {
        auto dep_it = m_states.find(dep);
        if (dep_it == m_states.end() || dep_it->second.status != DaemonStatus::Running) {
            log::warn("Cannot start service {}: Hard dependency {} is not running", service_id, dep);
            return false;
        }
    }

    state.status = DaemonStatus::Starting;
    EventJournal::instance().log_event(service_id, "STATE_CHANGE", "STARTING");

    pid_t pid = fork();
    if (pid == 0) {
        // Child Process: Set parent death signal so child dies if supervisor dies
        prctl(PR_SET_PDEATHSIG, SIGTERM);

        setenv("WAYLAND_DISPLAY", "wayland-0", 1);

        const char* home = std::getenv("HOME");
        if (home) {
            std::string lib_path = std::string(home) + "/tinexus/build/debug/src/common";
            const char* old_ld = std::getenv("LD_LIBRARY_PATH");
            std::string new_ld = old_ld ? lib_path + ":" + old_ld : lib_path;
            setenv("LD_LIBRARY_PATH", new_ld.c_str(), 1);
        }

        std::string resolved_path = resolve_binary_path(state.spec.executable.string());

        // Prepare exec arguments
        std::vector<char*> args;
        args.push_back(const_cast<char*>(resolved_path.c_str()));
        for (const auto& arg : state.spec.arguments) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        execvp(resolved_path.c_str(), args.data());
        _exit(127);
    } else if (pid > 0) {
        state.pid = pid;
        m_pid_map[pid] = service_id;
        state.status = DaemonStatus::Ready;
        EventJournal::instance().log_event(service_id, "STATE_CHANGE", "READY");

        state.status = DaemonStatus::Running;
        EventJournal::instance().log_event(service_id, "STATE_CHANGE", "RUNNING");
        HeartbeatWatchdog::instance().register_heartbeat(service_id);

        log::info("Supervised service started: {} (PID: {})", service_id, pid);
        return true;
    }

    state.status = DaemonStatus::Failed;
    EventJournal::instance().log_event(service_id, "STATE_CHANGE", "FAILED");
    return false;
}

bool ProcessManager::stop_service(const std::string& service_id) {
    auto it = m_states.find(service_id);
    if (it == m_states.end()) return false;

    auto& state = it->second;
    if (state.pid > 0) {
        state.status = DaemonStatus::Stopping;
        EventJournal::instance().log_event(service_id, "STATE_CHANGE", "STOPPING");

        kill(state.pid, SIGTERM);
        m_pid_map.erase(state.pid);
        state.pid = -1;
        state.status = DaemonStatus::Stopped;
        EventJournal::instance().log_event(service_id, "STATE_CHANGE", "STOPPED");
    }

    return true;
}

bool ProcessManager::restart_service(const std::string& service_id, RestartReason reason) {
    log::info("Restarting service {}: Reason {}", service_id, restart_reason_to_string(reason));
    EventJournal::instance().log_event(service_id, "RESTART", restart_reason_to_string(reason));

    stop_service(service_id);
    return start_service(service_id);
}

bool ProcessManager::start_all_services() {
    if (m_graph.has_cycle()) {
        log::error("Circular dependency detected in service graph! Refusing startup.");
        return false;
    }

    auto startup_order = m_graph.get_startup_order();
    for (const auto& id : startup_order) {
        start_service(id);
    }
    return true;
}

bool ProcessManager::stop_all_services() {
    auto shutdown_order = m_graph.get_shutdown_order();
    for (const auto& id : shutdown_order) {
        stop_service(id);
    }
    return true;
}

void ProcessManager::handle_child_exit(pid_t pid, int exit_code, int signal) {
    auto it = m_pid_map.find(pid);
    if (it == m_pid_map.end()) return;

    std::string service_id = it->second;
    m_pid_map.erase(it);

    auto state_it = m_states.find(service_id);
    if (state_it == m_states.end()) return;

    auto& state = state_it->second;
    state.pid = -1;
    state.restart_attempts++;

    EventJournal::instance().log_event(service_id, "CRASH", "PID " + std::to_string(pid) + " exited with code " + std::to_string(exit_code));
    log::warn("Supervised service crashed: {} (PID: {}, Exit: {}, Signal: {})", service_id, pid, exit_code, signal);

    if (!state.spec.critical && state.restart_attempts > state.spec.restart_limit) {
        state.status = DaemonStatus::Failed;
        EventJournal::instance().log_event(service_id, "STATE_CHANGE", "FAILED (Max restart limit exceeded)");
        log::error("Non-critical service {} reached max restart limit ({})", service_id, state.spec.restart_limit);
        return;
    }

    // Exponential Backoff Restart
    state.status = DaemonStatus::Restarting;
    EventJournal::instance().log_event(service_id, "STATE_CHANGE", "RESTARTING");

    uint32_t backoff_ms = calculate_backoff_ms(state.restart_attempts, state.spec.critical);
    log::info("Scheduling restart for {} in {} ms...", service_id, backoff_ms);

    if (backoff_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    }

    start_service(service_id);
}

void ProcessManager::handle_sighup_reload() {
    log::info("SIGHUP received. Reloading service manifests...");
    EventJournal::instance().log_event("serviced", "SIGHUP", "Reloading manifests");
}

std::unordered_map<std::string, ManagedServiceState> ProcessManager::get_all_states() const {
    return m_states;
}

std::optional<ManagedServiceState> ProcessManager::get_state(const std::string& service_id) const {
    auto it = m_states.find(service_id);
    if (it != m_states.end()) return it->second;
    return std::nullopt;
}

} // namespace tinexus::serviced
