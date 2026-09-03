#include "common/dbus_power.hpp"
#include "common/logger.hpp"
#include <systemd/sd-bus.h>

namespace tinexus::common::dbus_power {

static bool call_logind_method(const char* method_name) {
    sd_bus *bus = nullptr;
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        log::error("Failed to connect to system bus: {}", strerror(-r));
        return false;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = nullptr;

    r = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           method_name,
                           &error,
                           &m,
                           "b",
                           0); // interactive = false

    if (r < 0) {
        log::error("Failed to issue {} method call: {}", method_name, error.message);
        sd_bus_error_free(&error);
        sd_bus_flush_close_unref(bus);
        return false;
    }

    sd_bus_message_unref(m);
    sd_bus_flush_close_unref(bus);
    return true;
}

bool poweroff() {
    return call_logind_method("PowerOff");
}

bool reboot() {
    return call_logind_method("Reboot");
}

bool suspend() {
    return call_logind_method("Suspend");
}

bool logout() {
    // Terminate the user's session
    sd_bus *bus = nullptr;
    int r = sd_bus_open_system(&bus);
    if (r < 0) return false;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    
    // First we need to get the current session ID, but to keep it simple, 
    // many login1 systems support "auto" or we can call TerminateSession on Manager.
    // For Tinexus, let's call TerminateSession with an empty string which often targets the caller's session.
    r = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "TerminateSession",
                           &error,
                           nullptr,
                           "s",
                           "");

    if (r < 0) {
        log::error("Failed to terminate session: {}", error.message);
        sd_bus_error_free(&error);
    }
    
    sd_bus_flush_close_unref(bus);
    return r >= 0;
}

} // namespace tinexus::common::dbus_power
