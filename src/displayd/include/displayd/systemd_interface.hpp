#ifndef TINEXUS_DISPLAYD_SYSTEMD_INTERFACE_HPP
#define TINEXUS_DISPLAYD_SYSTEMD_INTERFACE_HPP

#include <string>

namespace tinexus::displayd {

class SystemdInterface {
public:
    static bool notify_ready();
    static bool notify_stopping();
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_SYSTEMD_INTERFACE_HPP
