#include "monitor/process_tree.hpp"
#include <common/PlatformServices.hpp>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <sstream>

namespace tinexus::monitor {

namespace {

std::string to_lower_ascii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string resolve_icon_for_process(const std::string& name_lower,
                                     const std::string& exe_lower,
                                     bool is_system,
                                     bool is_kernel_thread) {
    if (is_kernel_thread) {
        return "preferences-system-services";
    }
    if (name_lower.find("firefox") != std::string::npos || exe_lower.find("firefox") != std::string::npos) {
        return "firefox";
    }
    if (name_lower.find("chrom") != std::string::npos || exe_lower.find("chrom") != std::string::npos) {
        return "chromium";
    }
    if (name_lower == "tinexus-files" || exe_lower == "tinexus-files") {
        return "system-file-manager";
    }
    if (name_lower.rfind("tinexus-setting", 0) == 0 || exe_lower.rfind("tinexus-setting", 0) == 0) {
        return "preferences-system";
    }
    if (name_lower == "tinexus-terminal" || exe_lower == "tinexus-terminal" || name_lower == "foot" || name_lower == "bash" || name_lower == "zsh") {
        return "utilities-terminal";
    }
    if (name_lower == "tinexus-monitor" || exe_lower == "tinexus-monitor") {
        return "utilities-system-monitor";
    }
    if (name_lower == "tinexus-store" || exe_lower == "tinexus-store") {
        return "system-software-install";
    }
    if (name_lower.rfind("tinexus-", 0) == 0 || exe_lower.rfind("tinexus-", 0) == 0) {
        return "preferences-desktop";
    }
    if (name_lower.find("pipewire") != std::string::npos || name_lower.find("wireplumber") != std::string::npos || name_lower.find("pulse") != std::string::npos) {
        return "audio-card";
    }
    if (name_lower.find("networkmanager") != std::string::npos || name_lower.find("wpa_supplicant") != std::string::npos) {
        return "network-wireless";
    }
    if (is_system) {
        return "preferences-system-services";
    }
    return exe_lower.empty() ? name_lower : exe_lower;
}

} // namespace

ProcessTree::ProcessTree() {
    m_page_size = sysconf(_SC_PAGESIZE);
    if (m_page_size <= 0) m_page_size = 4096;
    m_current_uid = getuid();
}

uint64_t ProcessTree::read_total_jiffies() {
    int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;

    char buf[512];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (std::sscanf(buf, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                    &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    return 0;
}

std::string ProcessTree::resolve_username(uid_t uid) {
    auto it = m_uid_cache.find(uid);
    if (it != m_uid_cache.end()) {
        return it->second;
    }

    struct passwd* pw = getpwuid(uid);
    std::string name = pw ? pw->pw_name : std::to_string(uid);
    m_uid_cache[uid] = name;
    return name;
}

std::vector<ProcessInfo> ProcessTree::discover_processes() {
    std::vector<ProcessInfo> list;
    auto now = std::chrono::steady_clock::now();
    uint64_t curr_total_jiffies = read_total_jiffies();
    double elapsed_sec = m_has_prev_scan
                             ? std::chrono::duration<double>(now - m_prev_scan_time).count()
                             : 1.0;
    if (elapsed_sec <= 0.05) elapsed_sec = 1.0;

    uint64_t d_total_jiffies = (m_has_prev_scan && curr_total_jiffies >= m_prev_total_jiffies)
                                   ? (curr_total_jiffies - m_prev_total_jiffies)
                                   : 0;

    std::unordered_map<pid_t, PrevProcStats> next_proc_stats;
    next_proc_stats.reserve(m_prev_proc_stats.size() + 32);

    DIR* dir = opendir("/proc");
    if (!dir) return list;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type != DT_DIR) continue;
        if (!std::isdigit(static_cast<unsigned char>(ent->d_name[0]))) continue;

        pid_t pid = std::atoi(ent->d_name);
        if (pid <= 0) continue;

        char path_buf[128];
        char read_buf[1024];

        // 1. Single /proc/[pid]/stat read for comm, state, ppid, utime, stime, num_threads, rss
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/stat", pid);
        int fd_stat = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_stat < 0) continue;

        ssize_t n_stat = read(fd_stat, read_buf, sizeof(read_buf) - 1);
        close(fd_stat);
        if (n_stat <= 0) continue;
        read_buf[n_stat] = '\0';

        char* lparen = std::strchr(read_buf, '(');
        char* rparen = std::strrchr(read_buf, ')');
        if (!lparen || !rparen || rparen <= lparen) continue;

        *rparen = '\0';
        std::string proc_name(lparen + 1);
        *rparen = ')';

        const char* p_after = rparen + 2;

        char state_ch = 'S';
        int ppid = 0;
        unsigned long utime = 0, stime = 0;
        long num_threads = 1;
        long rss_pages = 0;
        if (std::sscanf(p_after, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %ld %*d %*u %*u %ld",
                        &state_ch, &ppid, &utime, &stime, &num_threads, &rss_pages) < 4) {
            continue;
        }

        uint64_t rss_bytes = (rss_pages > 0) ? (static_cast<uint64_t>(rss_pages) * static_cast<uint64_t>(m_page_size)) : 0;

        // 2. Fast UID lookup via fstatat on dirfd
        uid_t real_uid = 1000;
        struct stat st{};
        if (fstatat(dirfd(dir), ent->d_name, &st, 0) == 0) {
            real_uid = st.st_uid;
        }

        // 3. Full cmdline + untruncated basename resolution
        std::string cmdline;
        std::string exe_base;
        const bool is_kthread = (ppid == 2 || pid == 2);
        if (!is_kthread) {
            std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/cmdline", pid);
            int fd_cmd = open(path_buf, O_RDONLY | O_CLOEXEC);
            if (fd_cmd >= 0) {
                char cmd_buf[512];
                ssize_t n_cmd = read(fd_cmd, cmd_buf, sizeof(cmd_buf) - 1);
                close(fd_cmd);
                if (n_cmd > 0) {
                    std::string argv0(cmd_buf, strnlen(cmd_buf, static_cast<size_t>(n_cmd)));
                    auto slash = argv0.find_last_of('/');
                    exe_base = (slash != std::string::npos) ? argv0.substr(slash + 1) : argv0;
                    // Strip trailing arguments embedded in argv[0] if whitespace-separated
                    auto sp = exe_base.find(' ');
                    if (sp != std::string::npos) exe_base = exe_base.substr(0, sp);

                    for (ssize_t i = 0; i < n_cmd; ++i) {
                        cmdline.push_back(cmd_buf[i] == '\0' ? ' ' : cmd_buf[i]);
                    }
                    while (!cmdline.empty() && cmdline.back() == ' ') cmdline.pop_back();
                }
            }
            if (exe_base.empty()) {
                std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/exe", pid);
                char exe_buf[256];
                ssize_t n_exe = readlink(path_buf, exe_buf, sizeof(exe_buf) - 1);
                if (n_exe > 0) {
                    exe_buf[n_exe] = '\0';
                    std::string exe_str(exe_buf);
                    auto slash = exe_str.find_last_of('/');
                    exe_base = (slash != std::string::npos) ? exe_str.substr(slash + 1) : exe_str;
                }
            }
            // Recover kernel 15-char truncated comm when executable basename cleanly extends it
            if (proc_name.size() >= 15 && !exe_base.empty() &&
                to_lower_ascii(exe_base).rfind(to_lower_ascii(proc_name), 0) == 0) {
                proc_name = exe_base;
            }
        }

        // 4. Real per-process /proc/[pid]/io throughput parsing (graceful when non-root lacks EACCES)
        bool has_io = false;
        uint64_t curr_r_bytes = 0;
        uint64_t curr_w_bytes = 0;
        if (!is_kthread) {
            std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/io", pid);
            int fd_io = open(path_buf, O_RDONLY | O_CLOEXEC);
            if (fd_io >= 0) {
                char io_buf[512];
                ssize_t n_io = read(fd_io, io_buf, sizeof(io_buf) - 1);
                close(fd_io);
                if (n_io > 0) {
                    io_buf[n_io] = '\0';
                    const char* rb = std::strstr(io_buf, "read_bytes:");
                    const char* wb = std::strstr(io_buf, "write_bytes:");
                    if (rb && std::sscanf(rb + 11, "%lu", &curr_r_bytes) == 1 &&
                        wb && std::sscanf(wb + 12, "%lu", &curr_w_bytes) == 1) {
                        has_io = true;
                    }
                }
            }
        }

        // 5. CPU% and Disk Throughput Delta Calculation
        uint64_t curr_utime_stime = utime + stime;
        float cpu_pct = 0.0f;
        uint64_t read_rate = 0;
        uint64_t write_rate = 0;

        auto prev_it = m_prev_proc_stats.find(pid);
        if (prev_it != m_prev_proc_stats.end() && d_total_jiffies > 0) {
            uint64_t d_proc_ticks = (curr_utime_stime >= prev_it->second.utime_stime)
                                        ? (curr_utime_stime - prev_it->second.utime_stime)
                                        : 0;
            cpu_pct = std::clamp((static_cast<float>(d_proc_ticks) / static_cast<float>(d_total_jiffies)) * 100.0f,
                                 0.0f, 100.0f);

            if (has_io && elapsed_sec > 0.05) {
                uint64_t d_read = (curr_r_bytes >= prev_it->second.read_bytes)
                                      ? (curr_r_bytes - prev_it->second.read_bytes)
                                      : 0;
                uint64_t d_write = (curr_w_bytes >= prev_it->second.write_bytes)
                                       ? (curr_w_bytes - prev_it->second.write_bytes)
                                       : 0;
                read_rate = static_cast<uint64_t>(static_cast<double>(d_read) / elapsed_sec);
                write_rate = static_cast<uint64_t>(static_cast<double>(d_write) / elapsed_sec);
            }
        }

        next_proc_stats[pid] = {curr_utime_stime, curr_r_bytes, curr_w_bytes};

        // 6. Assemble ProcessInfo
        ProcessInfo info;
        info.pid = pid;
        info.ppid = ppid;
        info.uid = real_uid;
        info.threads = static_cast<uint32_t>(std::max<long>(1, num_threads));
        info.name = proc_name;
        info.cmdline = cmdline.empty() ? proc_name : cmdline;
        info.user = resolve_username(real_uid);
        info.rss_bytes = rss_bytes;
        info.cpu_percent = cpu_pct;
        info.read_bytes_sec = read_rate;
        info.write_bytes_sec = write_rate;
        info.has_io_permission = has_io;
        info.state = state_ch;

        switch (state_ch) {
            case 'R': info.state_str = "Running"; break;
            case 'S': info.state_str = "Sleeping"; break;
            case 'D': info.state_str = "Disk Sleep"; break;
            case 'Z': info.state_str = "Zombie"; break;
            case 'T': info.state_str = "Stopped"; break;
            case 'I': info.state_str = "Idle"; break;
            default:  info.state_str = "Sleeping"; break;
        }

        uint64_t total_io = read_rate + write_rate;
        if (cpu_pct >= 20.0f || total_io >= 5 * 1024 * 1024) {
            info.energy_impact = "High";
        } else if (cpu_pct >= 3.0f || total_io >= 500 * 1024) {
            info.energy_impact = "Moderate";
        } else {
            info.energy_impact = "Low";
        }

        if (pid == 1 || is_kthread || (!proc_name.empty() && proc_name.front() == '[') ||
            proc_name.rfind("tinexus-", 0) == 0 || proc_name.rfind("systemd", 0) == 0 ||
            real_uid == 0) {
            info.is_system_daemon = true;
        }

        info.icon_name = resolve_icon_for_process(
            to_lower_ascii(proc_name), to_lower_ascii(exe_base), info.is_system_daemon, is_kthread);

        list.push_back(std::move(info));
    }
    closedir(dir);

    m_prev_proc_stats = std::move(next_proc_stats);
    m_prev_total_jiffies = curr_total_jiffies;
    m_prev_scan_time = now;
    m_has_prev_scan = true;

    return list;
}

} // namespace tinexus::monitor
