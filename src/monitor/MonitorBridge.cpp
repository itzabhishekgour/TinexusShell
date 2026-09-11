#include "MonitorBridge.hpp"
#include "common/logger.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace tinexus::monitor {

MonitorBridge::MonitorBridge(QObject* parent)
    : QObject(parent)
    , m_currentUid(geteuid())
{
    // 1. Read static CPU model
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0 || line.rfind("Hardware", 0) == 0) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    m_cpuModel = QString::fromStdString(line.substr(colon + 2)).trimmed();
                    break;
                }
            }
        }
    }
    if (m_cpuModel.isEmpty()) {
        m_cpuModel = QStringLiteral("x86_64 Multi-Core Processor");
    }

    // Clean up marketing cruft
    m_cpuModel.remove(QStringLiteral("(R)"));
    m_cpuModel.remove(QStringLiteral("(TM)"));
    m_cpuModel.remove(QStringLiteral("CPU "));
    m_cpuModel = m_cpuModel.simplified();

    refresh();
}

void MonitorBridge::refresh() {
    SystemSnapshot snap = ResourceMonitor::instance().collect_snapshot();
    updateTelemetry(snap);
    updateProcessesList();
    updateServicesList();
}

void MonitorBridge::updateTelemetry(const SystemSnapshot& snap) {
    // 1. CPU
    m_cpuUsage = snap.cpu_aggregate_usage_percent;
    m_cpuTemperature = snap.cpu_temperature_c;
    m_cpuCoresCount = std::max(1, static_cast<int>(snap.cpu_cores.size()));

    m_cpuHistory.append(m_cpuUsage);
    while (m_cpuHistory.size() > 60) {
        m_cpuHistory.removeFirst();
    }

    // 2. Memory
    const auto& mem = snap.memory;
    m_memUsedBytes = mem.used_ram_bytes;
    m_memTotalBytes = mem.total_ram_bytes;
    m_memUsedGb = formatBytes(mem.used_ram_bytes);
    m_memTotalGb = formatBytes(mem.total_ram_bytes);
    m_memUsagePct = (mem.total_ram_bytes > 0)
        ? (static_cast<qreal>(mem.used_ram_bytes) / static_cast<qreal>(mem.total_ram_bytes)) * 100.0
        : 0.0;

    m_memHistory.append(m_memUsagePct);
    while (m_memHistory.size() > 60) {
        m_memHistory.removeFirst();
    }

    // 3. Disk I/O
    const auto& disk = snap.disk;
    m_diskReadBytesSec = disk.read_bytes_sec;
    m_diskWriteBytesSec = disk.write_bytes_sec;
    m_diskReadRate = formatRate(disk.read_bytes_sec);
    m_diskWriteRate = formatRate(disk.write_bytes_sec);

    qreal totalDiskKb = static_cast<qreal>(disk.read_bytes_sec + disk.write_bytes_sec) / 1024.0;
    m_diskHistory.append(totalDiskKb);
    while (m_diskHistory.size() > 60) {
        m_diskHistory.removeFirst();
    }

    // 4. Network I/O
    const auto& net = snap.network;
    m_netRxBytesSec = net.rx_bytes_sec;
    m_netTxBytesSec = net.tx_bytes_sec;
    m_netRxRate = formatRate(net.rx_bytes_sec);
    m_netTxRate = formatRate(net.tx_bytes_sec);

    qreal totalNetKb = static_cast<qreal>(net.rx_bytes_sec + net.tx_bytes_sec) / 1024.0;
    m_netHistory.append(totalNetKb);
    while (m_netHistory.size() > 60) {
        m_netHistory.removeFirst();
    }

    // Cache latest discovered processes
    m_cachedProcesses = snap.processes;

    emit telemetryChanged();
    emit historyChanged();
}

void MonitorBridge::updateProcessesList() {
    std::vector<ProcessInfo> filtered;
    filtered.reserve(m_cachedProcesses.size());

    QString qLower = m_searchQuery.toLower().trimmed();

    for (const auto& proc : m_cachedProcesses) {
        // Category filtering: 0: All, 1: My Processes, 2: System
        if (m_category == 1 && proc.uid != m_currentUid) {
            continue;
        }
        if (m_category == 2 && !proc.is_system_daemon) {
            continue;
        }

        // Search query filtering
        if (!qLower.isEmpty()) {
            QString name = QString::fromStdString(proc.name).toLower();
            QString pidStr = QString::number(proc.pid);
            if (!name.contains(qLower) && !pidStr.contains(qLower)) {
                continue;
            }
        }

        filtered.push_back(proc);
    }

    // Sorting
    std::sort(filtered.begin(), filtered.end(), [this](const ProcessInfo& a, const ProcessInfo& b) {
        if (m_sortAscending) {
            if (m_sortColumn == "name") return a.name < b.name;
            if (m_sortColumn == "pid") return a.pid < b.pid;
            if (m_sortColumn == "user") return a.user < b.user;
            if (m_sortColumn == "cpu") return a.cpu_percent < b.cpu_percent;
            if (m_sortColumn == "memory") return a.rss_bytes < b.rss_bytes;
            if (m_sortColumn == "disk") return (a.read_bytes_sec + a.write_bytes_sec) < (b.read_bytes_sec + b.write_bytes_sec);
            if (m_sortColumn == "state") return a.state_str < b.state_str;
            return a.cpu_percent < b.cpu_percent;
        } else {
            if (m_sortColumn == "name") return a.name > b.name;
            if (m_sortColumn == "pid") return a.pid > b.pid;
            if (m_sortColumn == "user") return a.user > b.user;
            if (m_sortColumn == "cpu") return a.cpu_percent > b.cpu_percent;
            if (m_sortColumn == "memory") return a.rss_bytes > b.rss_bytes;
            if (m_sortColumn == "disk") return (a.read_bytes_sec + a.write_bytes_sec) > (b.read_bytes_sec + b.write_bytes_sec);
            if (m_sortColumn == "state") return a.state_str > b.state_str;
            return a.cpu_percent > b.cpu_percent;
        }
    });

    m_displayProcesses.clear();
    for (const auto& proc : filtered) {
        QVariantMap map;
        map[QStringLiteral("pid")] = proc.pid;
        map[QStringLiteral("ppid")] = proc.ppid;
        map[QStringLiteral("name")] = QString::fromStdString(proc.name);
        map[QStringLiteral("user")] = QString::fromStdString(proc.user);
        map[QStringLiteral("cpu")] = proc.cpu_percent;
        map[QStringLiteral("cpuStr")] = QString::asprintf("%.1f%%", static_cast<double>(proc.cpu_percent));
        map[QStringLiteral("rss")] = static_cast<qulonglong>(proc.rss_bytes);
        map[QStringLiteral("rssStr")] = formatBytes(proc.rss_bytes);
        map[QStringLiteral("diskStr")] = formatRate(proc.read_bytes_sec + proc.write_bytes_sec);
        map[QStringLiteral("state")] = QString::fromStdString(proc.state_str);
        map[QStringLiteral("isSystem")] = proc.is_system_daemon;
        map[QStringLiteral("isProtected")] = (proc.pid == 1) || tinexus::platform::PlatformServices::is_protected_pid(proc.pid);
        m_displayProcesses.append(map);
    }

    emit processesChanged();
}

void MonitorBridge::updateServicesList() {
    m_services.clear();
    m_cachedServices = tinexus::platform::PlatformServices::query_supervised_services();
    for (const auto& s : m_cachedServices) {
        QVariantMap map;
        map[QStringLiteral("name")] = QString::fromStdString(s.name);
        map[QStringLiteral("role")] = QString::fromStdString(s.role);
        map[QStringLiteral("pid")] = s.pid;
        map[QStringLiteral("active")] = s.active;
        map[QStringLiteral("status")] = s.active ? QStringLiteral("ACTIVE") : QStringLiteral("STANDBY");
        map[QStringLiteral("pidStr")] = s.active ? QStringLiteral("PID %1").arg(s.pid) : QStringLiteral("Supervised");
        map[QStringLiteral("isProtected")] = s.is_protected;
        m_services.append(map);
    }
    emit servicesChanged();
}

void MonitorBridge::setSearchQuery(const QString& q) {
    if (m_searchQuery != q) {
        m_searchQuery = q;
        emit filterChanged();
        updateProcessesList();
    }
}

void MonitorBridge::setCategory(int cat) {
    if (m_category != cat) {
        m_category = cat;
        emit filterChanged();
        updateProcessesList();
    }
}

void MonitorBridge::setSortColumn(const QString& col) {
    if (m_sortColumn != col) {
        m_sortColumn = col;
        emit filterChanged();
        updateProcessesList();
    }
}

void MonitorBridge::setSortAscending(bool asc) {
    if (m_sortAscending != asc) {
        m_sortAscending = asc;
        emit filterChanged();
        updateProcessesList();
    }
}

void MonitorBridge::toggleSort(const QString& col) {
    if (m_sortColumn == col) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = col;
        m_sortAscending = (col == "name" || col == "user");
    }
    emit filterChanged();
    updateProcessesList();
}

void MonitorBridge::selectProcess(int pid) {
    if (m_selectedPid == pid) return;
    m_selectedPid = pid;
    m_selectedProcessName.clear();
    m_selectedIsProtected = (pid == 1) || tinexus::platform::PlatformServices::is_protected_pid(pid);

    for (const auto& proc : m_cachedProcesses) {
        if (proc.pid == pid) {
            m_selectedProcessName = QString::fromStdString(proc.name);
            break;
        }
    }
    emit selectionChanged();
}

void MonitorBridge::openKillDialog(bool force) {
    if (m_selectedPid <= 0) return;
    m_killModalForce = force;
    m_killModalOpen = true;
    emit killModalChanged();
}

void MonitorBridge::closeKillDialog() {
    m_killModalOpen = false;
    emit killModalChanged();
}

bool MonitorBridge::confirmKillProcess() {
    if (m_selectedPid <= 0) return false;

    bool is_prot = (m_selectedPid == 1) || tinexus::platform::PlatformServices::is_protected_pid(m_selectedPid);
    if (is_prot) {
        log::warn("[ActivityMonitor] Attempt to terminate protected PID %d rejected by safety boundary", m_selectedPid);
        closeKillDialog();
        return false;
    }

    log::info("[ActivityMonitor] Terminating PID %d (%s) force=%d",
              m_selectedPid, m_selectedProcessName.toStdString().c_str(), m_killModalForce);

    bool ok = ProcessController::terminate_process(m_selectedPid, m_killModalForce);
    closeKillDialog();
    refresh();
    return ok;
}

bool MonitorBridge::restartService(const QString& name) {
    std::string sname = name.toStdString();
    log::info("[ActivityMonitor] Restart requested for platform service %s", sname.c_str());

    for (const auto& s : m_cachedServices) {
        if (s.name == sname) {
            if (s.is_protected) {
                log::warn("[ActivityMonitor] Platform core service %s is protected and cannot be restarted manually", sname.c_str());
                return false;
            }
            if (s.active && s.pid > 0) {
                // Terminate child; tinexus-serviced supervision tree will respawn it automatically
                ProcessController::terminate_process(s.pid, false);
            } else {
                // If inactive, spawn directly
                pid_t pid = fork();
                if (pid == 0) {
                    execlp(sname.c_str(), sname.c_str(), nullptr);
                    _exit(127);
                }
            }
            refresh();
            return true;
        }
    }
    return false;
}

QString MonitorBridge::formatBytes(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        return QString::asprintf("%.1f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    if (bytes >= 1024ULL * 1024ULL) {
        return QString::asprintf("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    if (bytes >= 1024ULL) {
        return QString::asprintf("%.0f KB", static_cast<double>(bytes) / 1024.0);
    }
    return QString::asprintf("%lu B", static_cast<unsigned long>(bytes));
}

QString MonitorBridge::formatRate(uint64_t bytesSec) {
    if (bytesSec >= 1024ULL * 1024ULL) {
        return QString::asprintf("%.1f MB/s", static_cast<double>(bytesSec) / (1024.0 * 1024.0));
    }
    return QString::asprintf("%.0f KB/s", static_cast<double>(bytesSec) / 1024.0);
}

} // namespace tinexus::monitor
