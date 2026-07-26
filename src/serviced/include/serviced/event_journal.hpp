#ifndef TINEXUS_SERVICED_EVENT_JOURNAL_HPP
#define TINEXUS_SERVICED_EVENT_JOURNAL_HPP

#include "serviced/daemon_spec.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <shared_mutex>

namespace tinexus::serviced {

struct EventLog {
    uint64_t timestamp{0};
    std::string service_id;
    std::string event_type; // "STATE_CHANGE", "CRASH", "RESTART", "HEARTBEAT"
    std::string details;
};

class EventJournal {
public:
    static EventJournal& instance() noexcept;

    void log_event(std::string service_id, std::string event_type, std::string details);
    std::vector<EventLog> get_recent_events(size_t limit = 50) const;

private:
    EventJournal() = default;
    mutable std::shared_mutex m_mutex;
    std::vector<EventLog> m_logs;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_EVENT_JOURNAL_HPP
