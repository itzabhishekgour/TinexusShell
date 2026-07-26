#ifndef TINEXUS_SERVICED_HEARTBEAT_WATCHDOG_HPP
#define TINEXUS_SERVICED_HEARTBEAT_WATCHDOG_HPP

#include "serviced/daemon_spec.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <functional>

namespace tinexus::serviced {

struct ServiceHealthRecord {
    std::string service_id;
    HealthLevel level{HealthLevel::Healthy};
    uint64_t last_heartbeat_time{0};
    uint64_t uptime_seconds{0};
    uint64_t rss_bytes{0};
    uint32_t restart_count{0};
};

class HeartbeatWatchdog {
public:
    static HeartbeatWatchdog& instance() noexcept;

    void register_heartbeat(const std::string& service_id);
    void update_service_metrics(const std::string& service_id, uint64_t rss_bytes, uint64_t uptime_s);
    
    [[nodiscard]] ServiceHealthRecord get_health(const std::string& service_id) const;
    [[nodiscard]] std::vector<ServiceHealthRecord> get_all_health() const;

    void check_timeouts(const std::function<void(const std::string& service_id)>& on_timeout);

private:
    HeartbeatWatchdog() = default;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, ServiceHealthRecord> m_records;
    uint32_t m_timeout_seconds{10};
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_HEARTBEAT_WATCHDOG_HPP
