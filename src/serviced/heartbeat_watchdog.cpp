#include "serviced/heartbeat_watchdog.hpp"
#include "serviced/event_journal.hpp"
#include "common/logger.hpp"
#include <mutex>
#include <chrono>

namespace tinexus::serviced {

HeartbeatWatchdog& HeartbeatWatchdog::instance() noexcept {
    static HeartbeatWatchdog s_instance;
    return s_instance;
}

void HeartbeatWatchdog::register_heartbeat(const std::string& service_id) {
    std::unique_lock lock(m_mutex);
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    auto& record = m_records[service_id];
    record.service_id = service_id;
    record.last_heartbeat_time = now;
    record.level = HealthLevel::Healthy;
}

void HeartbeatWatchdog::update_service_metrics(const std::string& service_id, uint64_t rss_bytes, uint64_t uptime_s) {
    std::unique_lock lock(m_mutex);
    auto& record = m_records[service_id];
    record.service_id = service_id;
    record.rss_bytes = rss_bytes;
    record.uptime_seconds = uptime_s;
}

ServiceHealthRecord HeartbeatWatchdog::get_health(const std::string& service_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_records.find(service_id);
    if (it != m_records.end()) return it->second;
    return ServiceHealthRecord{service_id, HealthLevel::Critical, 0, 0, 0, 0};
}

std::vector<ServiceHealthRecord> HeartbeatWatchdog::get_all_health() const {
    std::shared_lock lock(m_mutex);
    std::vector<ServiceHealthRecord> results;
    results.reserve(m_records.size());
    for (const auto& [id, record] : m_records) {
        results.push_back(record);
    }
    return results;
}

void HeartbeatWatchdog::check_timeouts(const std::function<void(const std::string& service_id)>& on_timeout) {
    std::unique_lock lock(m_mutex);
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    for (auto& [id, record] : m_records) {
        if (record.last_heartbeat_time > 0 && (now - record.last_heartbeat_time) > m_timeout_seconds) {
            record.level = HealthLevel::Critical;
            EventJournal::instance().log_event(id, "HEARTBEAT_TIMEOUT", "No heartbeat received for 10s");
            log::warn("Service {} missed heartbeat (> 10s)", id);
            on_timeout(id);
        }
    }
}

} // namespace tinexus::serviced
