#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace tinexus::platform {

struct ServiceInfo {
    std::string name;
    std::string role;
    pid_t pid{0};
    bool active{false};
    std::string status_str{"STANDBY"};
    bool is_protected{false}; // True for tinexus-serviced and tinexus-comp (cannot be killed/restarted)
};

class PlatformServices {
public:
    /**
     * @brief Discovers supervised Tinexus platform services and their active PIDs by scanning /proc/[pid]/comm.
     */
    static std::vector<ServiceInfo> query_supervised_services();

    /**
     * @brief Looks up a specific process PID by its comm executable name.
     */
    static pid_t find_pid_by_name(const std::string& comm_name);

    /**
     * @brief Checks if a specific process PID belongs to a protected core platform daemon (or PID 1).
     */
    static bool is_protected_pid(pid_t pid);
};

} // namespace tinexus::platform
