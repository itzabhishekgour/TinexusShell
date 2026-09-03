#ifndef TINEXUS_SERVICED_LOGIND_MIMIC_HPP
#define TINEXUS_SERVICED_LOGIND_MIMIC_HPP

#include <systemd/sd-bus.h>

namespace tinexus::serviced {

class LogindMimic {
public:
    static LogindMimic& instance() {
        static LogindMimic inst;
        return inst;
    }

    LogindMimic(const LogindMimic&) = delete;
    LogindMimic& operator=(const LogindMimic&) = delete;

    // Connects to system bus and requests org.freedesktop.login1
    bool start();
    void stop();
    
    // Returns the dbus fd for polling
    int get_fd() const;
    void process_pending();

private:
    LogindMimic() = default;
    ~LogindMimic();

    sd_bus* m_bus{nullptr};
    sd_bus_slot* m_slot{nullptr};

public:
    // D-Bus Method Callbacks
    static int method_poweroff(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_reboot(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_suspend(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_terminate_session(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_LOGIND_MIMIC_HPP
