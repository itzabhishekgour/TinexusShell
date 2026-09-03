#ifndef TINEXUS_NOTIFICATIONS_DBUS_SERVER_HPP
#define TINEXUS_NOTIFICATIONS_DBUS_SERVER_HPP

#include <systemd/sd-bus.h>
#include <string>

namespace tinexus::notifications {

class DBusServer {
public:
    static DBusServer& instance();

    bool start();
    void stop();

    sd_bus* bus() const { return m_bus; }

    static int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
    static int method_close_notification(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
    static int method_get_capabilities(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
    static int method_get_server_information(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

private:
    DBusServer() = default;
    ~DBusServer();

    sd_bus* m_bus{nullptr};
    sd_bus_slot* m_slot{nullptr};
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATIONS_DBUS_SERVER_HPP
