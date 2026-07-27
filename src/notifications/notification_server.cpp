#include "notifications/notification_server.hpp"
#include "notifications/notification_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::notifications {

NotificationServer& NotificationServer::instance() noexcept {
    static NotificationServer s_instance;
    return s_instance;
}

std::vector<std::string> NotificationServer::get_capabilities() const {
    return {
        "actions",
        "body",
        "body-markup",
        "body-images",
        "persistence",
        "action-icons"
    };
}

ServerInformation NotificationServer::get_server_information() const {
    return ServerInformation{};
}

NotificationId NotificationServer::notify(const std::string& app_name,
                                           NotificationId replaces_id,
                                           const std::string& app_icon,
                                           const std::string& summary,
                                           const std::string& body,
                                           const std::vector<NotificationAction>& actions,
                                           Urgency urgency,
                                           int32_t expire_timeout_ms) {
    NotificationItem item;
    item.app_name = app_name;
    item.replaces_id = replaces_id;
    item.app_icon = app_icon;
    item.summary = summary;
    item.body = body;
    item.actions = actions;
    item.urgency = urgency;
    item.expire_timeout_ms = expire_timeout_ms;

    return NotificationManager::instance().notify(item);
}

bool NotificationServer::close_notification(NotificationId id) {
    return NotificationManager::instance().close_notification(id, ClosedReason::ClosedByCall);
}

} // namespace tinexus::notifications
