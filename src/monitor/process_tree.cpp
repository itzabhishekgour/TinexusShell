#include "monitor/process_tree.hpp"
#include <common/PlatformServices.hpp>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <sstream>

namespace tinexus::monitor {

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
        if (!std::isdigit(ent->d_name[0])) continue;

        pid_t pid = std::atoi(ent->d_name);
        if (pid <= 0) continue;

        char path_buf[128];
        char read_buf[1024];

        // 1. /proc/[pid]/comm for process name
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/comm", pid);
        int fd_comm = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_comm < 0) continue; // Process exited (ENOENT)

        ssize_t n_comm = read(fd_comm, read_buf, sizeof(read_buf) - 1);
        close(fd_comm);
        if (n_comm <= 0) continue;
        read_buf[n_comm] = '\0';
        // Trim trailing newline
        if (n_comm > 0 && (read_buf[n_comm - 1] == '\n' || read_buf[n_comm - 1] == '\r')) {
            read_buf[n_comm - 1] = '\0';
        }
        std::string proc_name(read_buf);

        // 2. /proc/[pid]/stat for state, ppid, utime, stime
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/stat", pid);
        int fd_stat = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_stat < 0) continue;

        ssize_t n_stat = read(fd_stat, read_buf, sizeof(read_buf) - 1);
        close(fd_stat);
        if (n_stat <= 0) continue;
        read_buf[n_stat] = '\0';

        char* rparen = std::strrchr(read_buf, ')');
        if (!rparen) continue;
        const char* p_after = rparen + 2;

        char state_ch = 'S';
        int ppid = 0;
        unsigned long utime = 0, stime = 0;
        if (std::sscanf(p_after, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                        &state_ch, &ppid, &utime, &stime) < 4) {
            continue;
        }

        // 3. /proc/[pid]/statm for RSS resident pages
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/statm", pid);
        uint64_t rss_bytes = 0;
        int fd_statm = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_statm >= 0) {
            ssize_t n_statm = read(fd_statm, read_buf, sizeof(read_buf) - 1);
            close(fd_statm);
            if (n_statm > 0) {
                read_buf[n_statm] = '\0';
                unsigned long resident_pages = 0;
                if (std::sscanf(read_buf, "%*u %lu", &resident_pages) == 1) {
                    rss_bytes = static_cast<uint64_t>(resident_pages) * static_cast<uint64_t>(m_page_size);
                }
            }
        }

        // 4. /proc/[pid]/status for real UID
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/status", pid);
        uid_t real_uid = 0;
        int fd_status = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_status >= 0) {
            ssize_t n_status = read(fd_status, read_buf, sizeof(read_buf) - 1);
            close(fd_status);
            if (n_status > 0) {
                read_buf[n_status] = '\0';
                char* uid_line = std::strstr(read_buf, "Uid:\t");
                if (uid_line) {
                    unsigned int parsed_uid = 0;
                    if (std::sscanf(uid_line + 5, "%u", &parsed_uid) == 1) {
                        real_uid = static_cast<uid_t>(parsed_uid);
                    }
                }
            }
        }

        // 5. /proc/[pid]/io for disk read/write bytes (gracefully handle EACCES)
        std::snprintf(path_buf, sizeof(path_buf), "/proc/%d/io", pid);
        bool has_io = false;
        uint64_t curr_r_bytes = 0;
        uint64_t curr_w_bytes = 0;
        int fd_io = open(path_buf, O_RDONLY | O_CLOEXEC);
        if (fd_io >= 0) {
            ssize_t n_io = read(fd_io, read_buf, sizeof(read_buf) - 1);
            close(fd_io);
            if (n_io > 0) {
                read_buf[n_io] = '\0';
                has_io = true;
                char* r_pos = std::strstr(read_buf, "read_bytes: ");
                char* w_pos = std::strstr(read_buf, "write_bytes: ");
                if (r_pos) std::sscanf(r_pos + 12, "%lu", &curr_r_bytes);
                if (w_pos) std::sscanf(w_pos + 13, "%lu", &curr_w_bytes);
            }
        }

        // 6. CPU% and Disk Throughput Delta Calculation
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

        // 7. Assemble ProcessInfo
        ProcessInfo info;
        info.pid = pid;
        info.ppid = ppid;
        info.uid = real_uid;
        info.name = proc_name;
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

        // Heuristic energy impact
        uint64_t total_io = read_rate + write_rate;
        if (cpu_pct >= 20.0f || total_io >= 5 * 1024 * 1024) {
            info.energy_impact = "High";
        } else if (cpu_pct >= 3.0f || total_io >= 500 * 1024) {
            info.energy_impact = "Moderate";
        } else {
            info.energy_impact = "Low";
        }

        // Check if system daemon
        if (pid == 1 || ppid == 2 || (!proc_name.empty() && proc_name.front() == '[') ||
            proc_name.rfind("tinexus-", 0) == 0 || proc_name.rfind("systemd", 0) == 0) {
            info.is_system_daemon = true;
        }

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
