#include "common/PlatformServices.hpp"
#include <dirent.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unistd.h>
#include <climits>

namespace tinexus::platform {

pid_t PlatformServices::find_pid_by_name(const std::string& comm_name) {
    DIR* dir = opendir("/proc");
    if (!dir) return 0;

    struct dirent* ent;
    pid_t found_pid = 0;
    const std::string comm_prefix = comm_name.substr(0, 15);

    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_DIR) {
            std::string pid_str = ent->d_name;
            if (!pid_str.empty() && std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) {
                // 1. Unforgeable check: readlink on /proc/[pid]/exe
                char exe_buf[PATH_MAX];
                ssize_t len = readlink(("/proc/" + pid_str + "/exe").c_str(), exe_buf, sizeof(exe_buf) - 1);
                if (len > 0) {
                    exe_buf[len] = '\0';
                    std::string exe_path(exe_buf);
                    std::size_t slash = exe_path.find_last_of('/');
                    std::string bin_name = (slash != std::string::npos) ? exe_path.substr(slash + 1) : exe_path;
                    if (bin_name == comm_name) {
                        try {
                            found_pid = static_cast<pid_t>(std::stoi(pid_str));
                            break;
                        } catch (...) {}
                    }
                }

                // 2. Fallback check: /proc/[pid]/comm (matching full name or 15-char truncated kernel comm)
                if (found_pid == 0) {
                    std::ifstream cf("/proc/" + pid_str + "/comm");
                    std::string comm;
                    if (cf >> comm && (comm == comm_name || comm == comm_prefix)) {
                        try {
                            found_pid = static_cast<pid_t>(std::stoi(pid_str));
                            break;
                        } catch (...) {}
                    }
                }
            }
        }
    }
    closedir(dir);
    return found_pid;
}

std::vector<ServiceInfo> PlatformServices::query_supervised_services() {
    struct DaemonMeta {
        const char* bin;
        const char* role;
        bool is_protected;
    };

    static const DaemonMeta daemons[] = {
        {"tinexus-serviced", "Platform Supervisor (Supervision Tree)", true},
        {"tinexus-comp",     "Wayland Compositor & DRM Server",        true},
        {"tinexus-ipcd",     "IPC Broker & Router Daemon",             false},
        {"tinexus-searchd",  "Ranking Engine & Index Daemon",          false},
        {"tinexus-notif",    "Desktop Notification Daemon",            false},
        {"tinexus-clip",     "Clipboard History Daemon",               false},
        {"tinexus-settings", "Configuration Authority Daemon",         false}
    };

    std::vector<ServiceInfo> services;
    services.reserve(sizeof(daemons) / sizeof(daemons[0]));

    for (const auto& d : daemons) {
        ServiceInfo info;
        info.name = d.bin;
        info.role = d.role;
        info.is_protected = d.is_protected;
        info.pid = find_pid_by_name(d.bin);

        if (info.pid > 0) {
            info.active = true;
            info.status_str = "ACTIVE (PID " + std::to_string(info.pid) + ")";
        } else {
            info.active = false;
            info.status_str = "STANDBY";
        }
        services.push_back(info);
    }

    return services;
}

bool PlatformServices::is_protected_pid(pid_t pid) {
    if (pid <= 1) return true; // PID 1 (systemd/init) and PID 0 are always protected

    // Cross-check against live PIDs tracked by PlatformServices
    auto services = query_supervised_services();
    for (const auto& svc : services) {
        if (svc.is_protected && svc.active && svc.pid > 0 && svc.pid == pid) {
            return true;
        }
    }
    return false;
}

} // namespace tinexus::platform
