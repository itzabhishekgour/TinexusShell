#include "monitor/process_controller.hpp"
#include "common/logger.hpp"
#include <csignal>
#include <unistd.h>

namespace tinexus::monitor {

bool ProcessController::terminate_process(pid_t pid, bool force) {
    int sig = force ? SIGKILL : SIGTERM;
    log::info("ProcessController: Sending signal {} to PID {}", force ? "SIGKILL" : "SIGTERM", pid);
    return kill(pid, sig) == 0;
}

bool ProcessController::suspend_process(pid_t pid) {
    log::info("ProcessController: Sending SIGSTOP to PID {}", pid);
    return kill(pid, SIGSTOP) == 0;
}

bool ProcessController::resume_process(pid_t pid) {
    log::info("ProcessController: Sending SIGCONT to PID {}", pid);
    return kill(pid, SIGCONT) == 0;
}

} // namespace tinexus::monitor
