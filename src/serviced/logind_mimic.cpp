#include "serviced/logind_mimic.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/reboot.h>
#include <signal.h>
#include <linux/reboot.h>
#include <fcntl.h>
#include <string.h>

namespace tinexus::serviced {

static const sd_bus_vtable logind_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("PowerOff", "b", "", LogindMimic::method_poweroff, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Reboot", "b", "", LogindMimic::method_reboot, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Suspend", "b", "", LogindMimic::method_suspend, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("TerminateSession", "s", "", LogindMimic::method_terminate_session, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

LogindMimic::~LogindMimic() {
    stop();
}

bool LogindMimic::start() {
    int r = sd_bus_open_system(&m_bus);
    if (r < 0) {
        log::error("LogindMimic: Failed to connect to system bus: {}", strerror(-r));
        return false;
    }

    r = sd_bus_add_object_vtable(m_bus,
                                 &m_slot,
                                 "/org/freedesktop/login1",  /* object path */
                                 "org.freedesktop.login1.Manager",   /* interface name */
                                 logind_vtable,
                                 nullptr);
    if (r < 0) {
        log::error("LogindMimic: Failed to issue method call: {}", strerror(-r));
        return false;
    }

    r = sd_bus_request_name(m_bus, "org.freedesktop.login1", 0);
    if (r < 0) {
        log::error("LogindMimic: Failed to acquire service name: {}", strerror(-r));
        return false;
    }

    log::info("LogindMimic: Successfully claimed org.freedesktop.login1 on system bus");
    return true;
}

void LogindMimic::stop() {
    if (m_slot) {
        sd_bus_slot_unref(m_slot);
        m_slot = nullptr;
    }
    if (m_bus) {
        sd_bus_unref(m_bus);
        m_bus = nullptr;
    }
}

int LogindMimic::get_fd() const {
    if (!m_bus) return -1;
    return sd_bus_get_fd(m_bus);
}

void LogindMimic::process_pending() {
    if (!m_bus) return;
    for (;;) {
        int r = sd_bus_process(m_bus, nullptr);
        if (r < 0) {
            log::error("LogindMimic: Failed to process bus: {}", strerror(-r));
            break;
        }
        if (r == 0) {
            break;
        }
    }
}

static void graceful_shutdown_sequence() {
    tinexus::log::info("Initiating graceful shutdown sequence...");
    
    // 1. SIGTERM to all processes
    tinexus::log::info("Sending SIGTERM to all processes...");
    kill(-1, SIGTERM);
    
    // Grace period
    sleep(2);
    
    // 2. SIGKILL to remaining processes
    tinexus::log::info("Sending SIGKILL to remaining processes...");
    kill(-1, SIGKILL);
    
    // 3. Sync filesystems
    tinexus::log::info("Syncing filesystems...");
    sync();

    // In a real OS we'd remount-ro here, but sysrq magic or busybox is needed.
    // sync() is usually enough for simple ext4/squashfs setups, but we can do a remount.
    // system("mount -o remount,ro /"); // System call avoided for security
}

int LogindMimic::method_poweroff(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    int interactive;
    int r = sd_bus_message_read(m, "b", &interactive);
    if (r < 0) {
        return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "Invalid interactive flag");
    }

    tinexus::log::warn("LogindMimic: PowerOff requested via D-Bus");
    sd_bus_reply_method_return(m, "");
    
    graceful_shutdown_sequence();
    reboot(RB_POWER_OFF);
    return 1;
}

int LogindMimic::method_reboot(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    int interactive;
    int r = sd_bus_message_read(m, "b", &interactive);
    if (r < 0) {
        return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "Invalid interactive flag");
    }

    tinexus::log::warn("LogindMimic: Reboot requested via D-Bus");
    sd_bus_reply_method_return(m, "");

    graceful_shutdown_sequence();
    reboot(RB_AUTOBOOT);
    return 1;
}

int LogindMimic::method_suspend(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    int interactive;
    int r = sd_bus_message_read(m, "b", &interactive);
    if (r < 0) {
        return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "Invalid interactive flag");
    }

    tinexus::log::info("LogindMimic: Suspend requested via D-Bus");
    sd_bus_reply_method_return(m, "");

    int fd = open("/sys/power/state", O_WRONLY);
    if (fd >= 0) {
        write(fd, "mem", 3);
        close(fd);
    } else {
        tinexus::log::error("LogindMimic: Failed to open /sys/power/state");
    }
    return 1;
}

int LogindMimic::method_terminate_session(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    (void)userdata;
    const char* session_id;
    int r = sd_bus_message_read(m, "s", &session_id);
    if (r < 0) {
        return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "Invalid session ID string");
    }

    tinexus::log::info("LogindMimic: TerminateSession requested for '{}'", session_id);
    sd_bus_reply_method_return(m, "");
    
    // In Tinexus OS, terminating the session usually just shuts down the shell.
    // But since `tinexus-serviced` is PID 1, we shouldn't kill ourselves unless we want kernel panic.
    // We will just kill `tinexus-session` process.
    // The ProcessManager handles that. For now we just log.
    tinexus::log::warn("LogindMimic: TerminateSession ignored in PID 1 context.");
    return 1;
}

} // namespace tinexus::serviced
