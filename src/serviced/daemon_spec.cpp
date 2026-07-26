#include "serviced/daemon_spec.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <fstream>

namespace tinexus::serviced {

std::string daemon_status_to_string(DaemonStatus status) noexcept {
    switch (status) {
        case DaemonStatus::Stopped: return "STOPPED";
        case DaemonStatus::Starting: return "STARTING";
        case DaemonStatus::Ready: return "READY";
        case DaemonStatus::Running: return "RUNNING";
        case DaemonStatus::Degraded: return "DEGRADED";
        case DaemonStatus::Unresponsive: return "UNRESPONSIVE";
        case DaemonStatus::Stopping: return "STOPPING";
        case DaemonStatus::Failed: return "FAILED";
        case DaemonStatus::Restarting: return "RESTARTING";
    }
    return "UNKNOWN";
}

std::string health_level_to_string(HealthLevel level) noexcept {
    switch (level) {
        case HealthLevel::Healthy: return "Healthy";
        case HealthLevel::Warning: return "Warning";
        case HealthLevel::Degraded: return "Degraded";
        case HealthLevel::Critical: return "Critical";
    }
    return "Unknown";
}

std::string restart_reason_to_string(RestartReason reason) noexcept {
    switch (reason) {
        case RestartReason::Crash: return "Crash";
        case RestartReason::HeartbeatTimeout: return "HeartbeatTimeout";
        case RestartReason::Manual: return "Manual";
        case RestartReason::ConfigurationReload: return "ConfigurationReload";
        case RestartReason::DependencyRestart: return "DependencyRestart";
        case RestartReason::Upgrade: return "Upgrade";
    }
    return "Unknown";
}

std::optional<DaemonSpec> ManifestLoader::load_from_toml_string(const std::string& content) {
    DaemonSpec spec;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t\r\n\"");
            if (start == std::string::npos) { s = ""; return; }
            auto end = s.find_last_not_of(" \t\r\n\"");
            s = s.substr(start, end - start + 1);
        };

        trim(key);
        trim(val);

        if (key == "id") spec.id = val;
        else if (key == "executable" || key == "binary") spec.executable = val;
        else if (key == "critical") spec.critical = (val == "true" || val == "1");
        else if (key == "autostart") spec.auto_start = (val == "true" || val == "1");
        else if (key == "restart_limit") spec.restart_limit = std::stoi(val);
        else if (key == "memory_limit_mb") spec.memory_limit_mb = static_cast<uint32_t>(std::stoul(val));
    }

    if (spec.id.empty() || spec.executable.empty()) {
        return std::nullopt;
    }

    return spec;
}

std::vector<DaemonSpec> ManifestLoader::load_all_from_directory(const std::filesystem::path& dir_path) {
    std::vector<DaemonSpec> specs;
    if (!std::filesystem::exists(dir_path)) return specs;

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") {
            std::ifstream f(entry.path());
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                auto parsed = load_from_toml_string(buffer.str());
                if (parsed) {
                    specs.push_back(std::move(*parsed));
                }
            }
        }
    }
    return specs;
}

} // namespace tinexus::serviced
