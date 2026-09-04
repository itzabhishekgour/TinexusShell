#include "notifications/dbus_server.hpp"
#include "notifications/notification_server.hpp"
#include "common/logger.hpp"
#include <vector>

namespace tinexus::notifications {

static const sd_bus_vtable notifications_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Notify", "susssasa{sv}i", "u", DBusServer::method_notify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("CloseNotification", "u", "", DBusServer::method_close_notification, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetCapabilities", "", "as", DBusServer::method_get_capabilities, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetServerInformation", "", "ssss", DBusServer::method_get_server_information, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NotificationClosed", "uu", 0),
    SD_BUS_SIGNAL("ActionInvoked", "us", 0),
    SD_BUS_VTABLE_END
};

DBusServer& DBusServer::instance() {
    static DBusServer s_instance;
    return s_instance;
}

DBusServer::~DBusServer() {
    stop();
}

bool DBusServer::start() {
    int r = sd_bus_open_user(&m_bus);
    if (r < 0) {
        log::error("Failed to connect to session bus: {}", strerror(-r));
        return false;
    }

    r = sd_bus_add_object_vtable(m_bus,
                                 &m_slot,
                                 "/org/freedesktop/Notifications",
                                 "org.freedesktop.Notifications",
                                 notifications_vtable,
                                 nullptr);
    if (r < 0) {
        log::error("Failed to add object vtable: {}", strerror(-r));
        return false;
    }

    r = sd_bus_request_name(m_bus, "org.freedesktop.Notifications", 0);
    if (r < 0) {
        log::error("Failed to acquire service name: {}", strerror(-r));
        return false;
    }

    log::info("DBusServer started on org.freedesktop.Notifications");
    return true;
}

void DBusServer::stop() {
    if (m_slot) {
        sd_bus_slot_unref(m_slot);
        m_slot = nullptr;
    }
    if (m_bus) {
        sd_bus_flush_close_unref(m_bus);
        m_bus = nullptr;
    }
}

void DBusServer::emit_notification_closed(uint32_t id, uint32_t reason) {
    if (!m_bus) return;
    sd_bus_emit_signal(m_bus, "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed", "uu", id, reason);
}

void DBusServer::emit_action_invoked(uint32_t id, const std::string& action_key) {
    if (!m_bus) return;
    sd_bus_emit_signal(m_bus, "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActionInvoked", "us", id, action_key.c_str());
}

int DBusServer::method_notify(sd_bus_message *m, void *, sd_bus_error *ret_error) {
    const char *app_name;
    uint32_t replaces_id;
    const char *app_icon;
    const char *summary;
    const char *body;
    
    int r = sd_bus_message_read(m, "susss", &app_name, &replaces_id, &app_icon, &summary, &body);
    if (r < 0) return r;

    // Read actions (as)
    std::vector<NotificationAction> action_list;
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) return r;
    
    while (true) {
        const char *action_id;
        r = sd_bus_message_read(m, "s", &action_id);
        if (r <= 0) break;
        
        const char *action_label;
        r = sd_bus_message_read(m, "s", &action_label);
        if (r <= 0) break;
        
        NotificationAction action;
        action.action_key = action_id;
        action.label = action_label;
        action_list.push_back(action);
    }
    sd_bus_message_exit_container(m);

    // Read hints a{sv}
    Urgency urgency = Urgency::Normal;
    
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
    if (r < 0) return r;
    
    while (true) {
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv");
        if (r <= 0) break;
        
        const char *dict_key;
        r = sd_bus_message_read(m, "s", &dict_key);
        if (r < 0) return r;
        
        if (std::string(dict_key) == "urgency") {
            r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "y");
            if (r >= 0) {
                uint8_t u;
                sd_bus_message_read(m, "y", &u);
                if (u == 0) urgency = Urgency::Low;
                else if (u == 1) urgency = Urgency::Normal;
                else if (u == 2) urgency = Urgency::Critical;
                sd_bus_message_exit_container(m);
            } else {
                sd_bus_message_skip(m, "v");
            }
        } else {
            sd_bus_message_skip(m, "v");
        }
        
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);

    // Read expire_timeout
    int32_t expire_timeout;
    r = sd_bus_message_read(m, "i", &expire_timeout);
    if (r < 0) return r;

    NotificationId id = NotificationServer::instance().notify(
        app_name ? app_name : "",
        replaces_id,
        app_icon ? app_icon : "",
        summary ? summary : "",
        body ? body : "",
        action_list,
        urgency,
        expire_timeout
    );

    return sd_bus_reply_method_return(m, "u", id);
}

int DBusServer::method_close_notification(sd_bus_message *m, void *, sd_bus_error *ret_error) {
    uint32_t id;
    int r = sd_bus_message_read(m, "u", &id);
    if (r < 0) return r;

    NotificationServer::instance().close_notification(id);
    
    return sd_bus_reply_method_return(m, "");
}

int DBusServer::method_get_capabilities(sd_bus_message *m, void *, sd_bus_error *ret_error) {
    sd_bus_message *reply;
    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    if (r < 0) return r;

    auto caps = NotificationServer::instance().get_capabilities();
    for (const auto& cap : caps) {
        sd_bus_message_append(reply, "s", cap.c_str());
    }

    r = sd_bus_message_close_container(reply);
    if (r < 0) return r;

    return sd_bus_send(nullptr, reply, nullptr);
}

int DBusServer::method_get_server_information(sd_bus_message *m, void *, sd_bus_error *ret_error) {
    auto info = NotificationServer::instance().get_server_information();
    return sd_bus_reply_method_return(m, "ssss", 
                                      info.name.c_str(), 
                                      info.vendor.c_str(), 
                                      info.version.c_str(), 
                                      info.spec_version.c_str());
}

} // namespace tinexus::notifications
