#ifndef TINEXUS_NOTIFICATIONS_DND_MANAGER_HPP
#define TINEXUS_NOTIFICATIONS_DND_MANAGER_HPP

#include "notifications/notification_item.hpp"

namespace tinexus::notifications {

class DndManager {
public:
    static DndManager& instance() noexcept;

    DndManager() = default;
    ~DndManager() = default;

    void set_dnd_reason(DndReason reason) noexcept { m_reason = reason; }
    [[nodiscard]] DndReason get_dnd_reason() const noexcept { return m_reason; }
    [[nodiscard]] bool is_dnd_active() const noexcept { return m_reason != DndReason::None; }

    bool should_suppress_popup(Urgency urgency) const noexcept;

private:
    DndReason m_reason{DndReason::None};
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATIONS_DND_MANAGER_HPP
