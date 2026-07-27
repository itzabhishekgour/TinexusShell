#include "notifications/notification_manager.hpp"
#include "notifications/dnd_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::notifications {

NotificationManager& NotificationManager::instance() noexcept {
    static NotificationManager s_instance;
    return s_instance;
}

NotificationId NotificationManager::notify(NotificationItem item) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Handle replacement if replaces_id != 0
    if (item.replaces_id != 0) {
        for (auto& active : m_active_queue) {
            if (active.id == item.replaces_id) {
                log::info("NotificationManager: Replacing notification ID={}", item.replaces_id);
                item.id = item.replaces_id;
                item.state = NotificationState::Displayed;
                active = item;
                return item.id;
            }
        }
    }

    // Assign new unique ID
    item.id = m_next_id.fetch_add(1);
    item.state = NotificationState::Displayed;

    // Check DND Policy
    if (DndManager::instance().should_suppress_popup(item.urgency)) {
        log::info("NotificationManager: DND Active. Suppressing popup for ID={} (urgency={})", item.id, static_cast<int>(item.urgency));
    }

    m_active_queue.push_back(item);

    // Push into ring buffer history
    m_history_ring.push_back(item);
    if (m_history_ring.size() > m_history_capacity) {
        m_history_ring.pop_front();
    }

    log::info("NotificationManager: Created notification ID={} ('{}': '{}')", item.id, item.app_name, item.summary);
    return item.id;
}

bool NotificationManager::close_notification(NotificationId id, ClosedReason reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_active_queue.begin(); it != m_active_queue.end(); ++it) {
        if (it->id == id) {
            it->state = NotificationState::Closed;
            m_active_queue.erase(it);
            log::info("NotificationManager: Closed notification ID={} (reason={})", id, static_cast<int>(reason));
            return true;
        }
    }
    return false;
}

std::optional<NotificationItem> NotificationManager::get_notification(NotificationId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& item : m_active_queue) {
        if (item.id == id) return item;
    }
    return std::nullopt;
}

std::vector<NotificationItem> NotificationManager::active_queue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active_queue;
}

std::vector<NotificationItem> NotificationManager::history() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<NotificationItem>(m_history_ring.begin(), m_history_ring.end());
}

} // namespace tinexus::notifications
