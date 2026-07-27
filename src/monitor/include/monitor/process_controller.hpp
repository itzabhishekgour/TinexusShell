#ifndef TINEXUS_MONITOR_PROCESS_CONTROLLER_HPP
#define TINEXUS_MONITOR_PROCESS_CONTROLLER_HPP

#include <cstdint>
#include <sys/types.h>

namespace tinexus::monitor {

class ProcessController {
public:
    static bool terminate_process(pid_t pid, bool force = false);
    static bool suspend_process(pid_t pid);
    static bool resume_process(pid_t pid);
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_PROCESS_CONTROLLER_HPP
