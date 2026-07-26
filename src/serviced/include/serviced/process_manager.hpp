#ifndef TINEXUS_SERVICED_PROCESS_MANAGER_HPP
#define TINEXUS_SERVICED_PROCESS_MANAGER_HPP

#include "serviced/daemon_spec.hpp"
#include "serviced/dep_graph.hpp"
#include <unordered_map>
#include <memory>
#include <sys/types.h>

namespace tinexus::serviced {

struct ManagedServiceState {
    DaemonSpec spec;
    DaemonStatus status{DaemonStatus::Stopped};
    pid_t pid{-1};
    int restart_attempts{0};
    uint64_t last_start_time{0};
};

class ProcessManager {
public:
    explicit ProcessManager(DependencyGraph graph);
    ~ProcessManager();

    bool start_all_services();
    bool stop_all_services();

    bool start_service(const std::string& service_id);
    bool stop_service(const std::string& service_id);
    bool restart_service(const std::string& service_id, RestartReason reason);

    void handle_child_exit(pid_t pid, int exit_code, int signal);
    void handle_sighup_reload();

    [[nodiscard]] std::unordered_map<std::string, ManagedServiceState> get_all_states() const;
    [[nodiscard]] std::optional<ManagedServiceState> get_state(const std::string& service_id) const;

private:
    DependencyGraph m_graph;
    std::unordered_map<std::string, ManagedServiceState> m_states;
    std::unordered_map<pid_t, std::string> m_pid_map;

    uint32_t calculate_backoff_ms(int attempt, bool is_critical) noexcept;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_PROCESS_MANAGER_HPP
