#include "serviced/event_journal.hpp"
#include <mutex>
#include <chrono>

namespace tinexus::serviced {

EventJournal& EventJournal::instance() noexcept {
    static EventJournal s_instance;
    return s_instance;
}

void EventJournal::log_event(std::string service_id, std::string event_type, std::string details) {
    std::unique_lock lock(m_mutex);
    uint64_t ts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    m_logs.push_back({ts, std::move(service_id), std::move(event_type), std::move(details)});
    if (m_logs.size() > 500) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + 100);
    }
}

std::vector<EventLog> EventJournal::get_recent_events(size_t limit) const {
    std::shared_lock lock(m_mutex);
    if (m_logs.size() <= limit) return m_logs;
    return std::vector<EventLog>(m_logs.end() - static_cast<ptrdiff_t>(limit), m_logs.end());
}

} // namespace tinexus::serviced
