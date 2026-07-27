#ifndef TINEXUS_NOTIFICATIONS_NOTIFICATION_MANAGER_HPP
#define TINEXUS_NOTIFICATIONS_NOTIFICATION_MANAGER_HPP

#include "notifications/notification_item.hpp"
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <optional>

namespace tinexus::notifications {

class NotificationManager {
public:
    static NotificationManager& instance() noexcept;

    NotificationManager() = default;
    ~NotificationManager() = default;

    NotificationId notify(NotificationItem item);
    bool close_notification(NotificationId id, ClosedReason reason);
    std::optional<NotificationItem> get_notification(NotificationId id) const;

    [[nodiscard]] std::vector<NotificationItem> active_queue() const;
    [[nodiscard]] std::vector<NotificationItem> history() const;

    void set_history_capacity(size_t capacity) { m_history_capacity = capacity; }

private:
    std::atomic<NotificationId> m_next_id{1};
    mutable std::mutex m_mutex;
    std::vector<NotificationItem> m_active_queue;
    std::deque<NotificationItem> m_history_ring;
    size_t m_history_capacity{500};
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATIONS_NOTIFICATION_MANAGER_HPP
