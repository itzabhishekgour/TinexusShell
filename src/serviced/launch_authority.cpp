#include "serviced/launch_authority.hpp"
#include "serviced/event_journal.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

namespace tinexus::serviced {

LaunchAuthority& LaunchAuthority::instance() noexcept {
    static LaunchAuthority s_instance;
    return s_instance;
}

bool LaunchAuthority::is_valid_executable(const std::string& exec_cmd) const {
    if (exec_cmd.empty()) return false;
    // Basic safety check: reject malicious injection characters
    if (exec_cmd.find(';') != std::string::npos || exec_cmd.find('&') != std::string::npos ||
        exec_cmd.find('|') != std::string::npos || exec_cmd.find('`') != std::string::npos) {
        return false;
    }
    return true;
}

pid_t LaunchAuthority::launch_app(const std::string& app_id, const std::string& exec_cmd) {
    if (!is_valid_executable(exec_cmd)) {
        log::error("LaunchAuthority: Rejected invalid/unsafe exec string for app '{}': {}", app_id, exec_cmd);
        EventJournal::instance().log_event(app_id, "LAUNCH_REJECTED", "Unsafe exec command");
        return -1;
    }

    log::info("LaunchAuthority: Spawning application '{}' -> {}", app_id, exec_cmd);
    EventJournal::instance().log_event(app_id, "APP_LAUNCH", exec_cmd);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process: create new session group
        setsid();
        execl("/bin/sh", "sh", "-c", exec_cmd.c_str(), nullptr);
        _exit(127);
    }

    if (pid > 0) {
        log::info("LaunchAuthority: App '{}' launched successfully with PID {}", app_id, pid);
    } else {
        log::error("LaunchAuthority: Failed to fork app '{}'", app_id);
    }

    return pid;
}

} // namespace tinexus::serviced
