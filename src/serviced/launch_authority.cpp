#include "serviced/launch_authority.hpp"
#include "serviced/event_journal.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <vector>

namespace tinexus::serviced {

LaunchAuthority& LaunchAuthority::instance() noexcept {
    static LaunchAuthority s_instance;
    return s_instance;
}

bool LaunchAuthority::is_valid_executable(const std::string& exec_cmd) const {
    if (exec_cmd.empty()) return false;
    // Reject raw shell injection characters
    if (exec_cmd.find(';') != std::string::npos || exec_cmd.find('&') != std::string::npos ||
        exec_cmd.find('|') != std::string::npos || exec_cmd.find('`') != std::string::npos) {
        return false;
    }
    return true;
}

pid_t LaunchAuthority::execute_action(const ActionRequest& req) {
    log::info("LaunchAuthority: Executing ActionRequest Type {} -> Target: {}", static_cast<int>(req.type), req.target);
    EventJournal::instance().log_event("action_authority", "ACTION_EXEC", req.target);

    pid_t pid = fork();
    if (pid == 0) {
        setsid();

        if (!req.working_dir.empty()) {
            chdir(req.working_dir.c_str());
        }

        // Set environment variables if specified
        for (const auto& [k, v] : req.env) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        // Prepare execve arguments array cleanly without shell evaluation
        std::vector<char*> args;
        args.push_back(const_cast<char*>(req.target.c_str()));
        for (const auto& arg : req.arguments) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);

        execvp(req.target.c_str(), args.data());
        _exit(127);
    }

    if (pid > 0) {
        log::info("LaunchAuthority: Action executed successfully with PID {}", pid);
    } else {
        log::error("LaunchAuthority: Failed to fork action process for target '{}'", req.target);
    }

    return pid;
}

pid_t LaunchAuthority::launch_app(const std::string& app_id, const std::string& exec_cmd) {
    if (!is_valid_executable(exec_cmd)) {
        log::error("LaunchAuthority: Rejected invalid/unsafe exec string for app '{}': {}", app_id, exec_cmd);
        EventJournal::instance().log_event(app_id, "LAUNCH_REJECTED", "Unsafe exec command");
        return -1;
    }

    ActionRequest req;
    req.type = ActionType::AppLaunch;
    req.target = exec_cmd;
    return execute_action(req);
}

} // namespace tinexus::serviced
