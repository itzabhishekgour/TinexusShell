#ifndef TINEXUS_COMMON_DBUS_POWER_HPP
#define TINEXUS_COMMON_DBUS_POWER_HPP

namespace tinexus::common::dbus_power {

bool poweroff();
bool reboot();
bool suspend();
bool logout();

} // namespace tinexus::common::dbus_power

#endif // TINEXUS_COMMON_DBUS_POWER_HPP
