#include "notifications/dnd_manager.hpp"

namespace tinexus::notifications {

DndManager& DndManager::instance() noexcept {
    static DndManager s_instance;
    return s_instance;
}

bool DndManager::should_suppress_popup(Urgency urgency) const noexcept {
    if (!is_dnd_active()) return false;

    // Critical notifications ALWAYS bypass DND policies
    if (urgency == Urgency::Critical) return false;

    return true; // Low & Normal urgency popups are suppressed
}

} // namespace tinexus::notifications
