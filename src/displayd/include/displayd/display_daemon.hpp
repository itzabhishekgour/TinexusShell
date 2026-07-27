#ifndef TINEXUS_DISPLAYD_DISPLAY_DAEMON_HPP
#define TINEXUS_DISPLAYD_DISPLAY_DAEMON_HPP

#include "displayd/vt_manager.hpp"
#include "displayd/seat_manager.hpp"
#include "displayd/login_monitor.hpp"
#include "displayd/session_launcher.hpp"
#include "displayd/systemd_interface.hpp"

namespace tinexus::displayd {

class DisplayDaemon {
public:
    DisplayDaemon() = default;
    ~DisplayDaemon() = default;

    bool bootstrap();
    bool shutdown();

    [[nodiscard]] VtManager& vt() noexcept { return m_vt; }
    [[nodiscard]] SeatManager& seat() noexcept { return m_seat; }
    [[nodiscard]] LoginMonitor& login() noexcept { return m_login; }

private:
    VtManager m_vt;
    SeatManager m_seat;
    LoginMonitor m_login;
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_DISPLAY_DAEMON_HPP
