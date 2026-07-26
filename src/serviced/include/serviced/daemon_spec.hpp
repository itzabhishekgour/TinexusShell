#ifndef TINEXUS_SERVICED_DAEMON_SPEC_HPP
#define TINEXUS_SERVICED_DAEMON_SPEC_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <filesystem>
#include <optional>

namespace tinexus::serviced {

enum class DaemonStatus {
    Stopped,
    Starting,
    Ready,
    Running,
    Degraded,
    Unresponsive,
    Stopping,
    Failed,
    Restarting
};

enum class HealthLevel {
    Healthy,
    Warning,
    Degraded,
    Critical
};

enum class RestartReason {
    Crash,
    HeartbeatTimeout,
    Manual,
    ConfigurationReload,
    DependencyRestart,
    Upgrade
};

enum class ServiceCategory {
    Core,
    System,
    UI,
    Background,
    Optional
};

struct DaemonSpec {
    uint32_t manifest_version{1};
    std::string id;
    std::filesystem::path executable;
    std::vector<std::string> hard_dependencies;
    std::vector<std::string> soft_dependencies;
    bool auto_start{true};
    bool critical{false};
    bool restartable{true};
    std::chrono::milliseconds startup_timeout{5000};
    std::chrono::milliseconds shutdown_timeout{3000};
    int restart_limit{5};
    std::string working_directory;
    std::vector<std::string> arguments;
    std::map<std::string, std::string> environment;
    uint32_t memory_limit_mb{512};
    uint32_t cpu_limit_percent{100};
    uint32_t max_open_files{4096};
    ServiceCategory category{ServiceCategory::Core};
};

std::string daemon_status_to_string(DaemonStatus status) noexcept;
std::string health_level_to_string(HealthLevel level) noexcept;
std::string restart_reason_to_string(RestartReason reason) noexcept;

class ManifestLoader {
public:
    static std::optional<DaemonSpec> load_from_toml_string(const std::string& toml_content);
    static std::vector<DaemonSpec> load_all_from_directory(const std::filesystem::path& dir_path);
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_DAEMON_SPEC_HPP
