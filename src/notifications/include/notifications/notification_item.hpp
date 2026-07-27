#ifndef TINEXUS_NOTIFICATIONS_NOTIFICATION_ITEM_HPP
#define TINEXUS_NOTIFICATIONS_NOTIFICATION_ITEM_HPP

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace tinexus::notifications {

using NotificationId = uint64_t;

enum class Urgency : uint8_t {
    Low = 0,
    Normal = 1,
    Critical = 2
};

enum class NotificationCategory : uint8_t {
    System = 0,
    Media = 1,
    Chat = 2,
    Downloads = 3,
    Updates = 4,
    Security = 5,
    Calendar = 6
};

enum class NotificationState : uint8_t {
    Pending = 0,
    Displayed = 1,
    Waiting = 2,
    Expired = 3,
    Closed = 4
};

enum class ClosedReason : uint8_t {
    Expired = 1,
    Dismissed = 2,
    ClosedByCall = 3,
    Undefined = 4
};

enum class DndReason : uint8_t {
    None = 0,
    UserEnabled = 1,
    QuietHours = 2,
    Fullscreen = 3,
    PresentationMode = 4
};

struct NotificationAction {
    std::string action_key;
    std::string label;
};

struct NotificationItem {
    NotificationId id{0};
    NotificationId replaces_id{0};
    std::string app_name;
    std::string app_icon;
    std::string summary;
    std::string body;
    Urgency urgency{Urgency::Normal};
    NotificationCategory category{NotificationCategory::System};
    NotificationState state{NotificationState::Pending};
    int32_t expire_timeout_ms{5000}; // -1 = default, 0 = never expire
    std::vector<NotificationAction> actions;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATIONS_NOTIFICATION_ITEM_HPP
