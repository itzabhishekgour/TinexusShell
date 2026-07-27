#ifndef TINEXUS_NOTIFICATIONS_NOTIFICATION_SERVER_HPP
#define TINEXUS_NOTIFICATIONS_NOTIFICATION_SERVER_HPP

#include "notifications/notification_item.hpp"
#include <vector>
#include <string>

namespace tinexus::notifications {

struct ServerInformation {
    std::string name{"tinexus-notifications"};
    std::string vendor{"Tinexus Platform"};
    std::string version{"1.0.0"};
    std::string spec_version{"1.2"};
};

class NotificationServer {
public:
    static NotificationServer& instance() noexcept;

    NotificationServer() = default;
    ~NotificationServer() = default;

    std::vector<std::string> get_capabilities() const;
    ServerInformation get_server_information() const;

    NotificationId notify(const std::string& app_name,
                           NotificationId replaces_id,
                           const std::string& app_icon,
                           const std::string& summary,
                           const std::string& body,
                           const std::vector<NotificationAction>& actions,
                           Urgency urgency,
                           int32_t expire_timeout_ms);

    bool close_notification(NotificationId id);
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATIONS_NOTIFICATION_SERVER_HPP
