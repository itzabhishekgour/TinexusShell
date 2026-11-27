#include "MonitorBridge.hpp"
#include "common/logger.hpp"

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

namespace tinexus::monitor {

MonitorBridge::MonitorBridge(QObject* parent)
    : QObject(parent)
    , m_currentUid(geteuid())
{
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

    m_cpuModel.remove(QStringLiteral("(R)"));
    m_cpuModel.remove(QStringLiteral("(TM)"));
    m_cpuModel.remove(QStringLiteral("CPU "));
    m_cpuModel = m_cpuModel.simplified();

    // Synchronous initial baseline so first frame and live contract tests have data immediately
    refreshSync(true, true);
}

void MonitorBridge::setActiveTab(int tab) {
    if (m_activeTab != tab) {
        m_activeTab = tab;
        emit activeTabChanged();
        refresh();
    }
}

void MonitorBridge::setIsVisible(bool v) {
    if (m_isVisible != v) {
        m_isVisible = v;
        emit isVisibleChanged();
        if (m_isVisible) {
            refresh();
        }
    }
}

void MonitorBridge::appendRing(QVariantList& ring, qreal value, int maxLen) {
    ring.append(value);
    while (ring.size() > maxLen) {
        ring.removeFirst();
    }
}

void MonitorBridge::refreshSync(bool collectProcs, bool collectServices) {
    SystemSnapshot snap = ResourceMonitor::instance().collect_snapshot(collectProcs);
    applySnapshot(snap, collectProcs);
    if (collectServices) {
        applyServicesList(tinexus::platform::PlatformServices::query_supervised_services());
    }
}

void MonitorBridge::refresh() {
    if (!m_isVisible) {
        return; // Zero CPU polling while window is hidden or minimized
    }
    if (m_refreshing->exchange(true)) {
        return;
    }

    const bool needProcs = (m_activeTab == 0) || m_cachedProcesses.empty();
    const bool needServices = (m_activeTab == 2);
    QPointer<MonitorBridge> self(this);
    auto refreshingFlag = m_refreshing;

    std::thread([self, refreshingFlag, needProcs, needServices]() {
        SystemSnapshot snap = ResourceMonitor::instance().collect_snapshot(needProcs);
        std::vector<tinexus::platform::ServiceInfo> svcs;
        if (needServices) {
            svcs = tinexus::platform::PlatformServices::query_supervised_services();
        }
        if (!self) {
            refreshingFlag->store(false);
            return;
        }
        QMetaObject::invokeMethod(self, [self, refreshingFlag, snap = std::move(snap), svcs = std::move(svcs), needProcs, needServices]() {
            refreshingFlag->store(false);
            if (!self) return;
            self->applySnapshot(snap, needProcs);
            if (needServices) {
                self->applyServicesList(svcs);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void MonitorBridge::applySnapshot(const SystemSnapshot& snap, bool hasProcs) {
    // 1. CPU & Thermal
    m_cpuUsage = snap.cpu_aggregate_usage_percent;
    m_cpuTemperature = snap.cpu_temperature_c;
    m_cpuTemperatureStr = (snap.cpu_temperature_c > 0.0f)
        ? QString::asprintf("%.0f°C", static_cast<double>(snap.cpu_temperature_c))
        : QStringLiteral("N/A");
    m_cpuCoresCount = std::max(1, static_cast<int>(snap.cpu_cores.size()));
    appendRing(m_cpuHistory, m_cpuUsage);

    m_coreUsages.clear();
    m_coreUsages.reserve(static_cast<int>(snap.cpu_cores.size()));
    for (const auto& c : snap.cpu_cores) {
        QVariantMap cm;
        cm[QStringLiteral("core")] = static_cast<int>(c.core_id);
        cm[QStringLiteral("usage")] = static_cast<qreal>(c.usage_percent);
        m_coreUsages.append(cm);
    }

    // 2. Memory
    const auto& mem = snap.memory;
    m_memUsedBytes = mem.used_ram_bytes;
    m_memTotalBytes = mem.total_ram_bytes;
    m_memUsedGb = formatBytes(mem.used_ram_bytes);
    m_memTotalGb = formatBytes(mem.total_ram_bytes);
    m_memCachedGb = formatBytes(mem.cached_bytes + mem.buffers_bytes);
    m_memSwapUsedGb = formatBytes(mem.used_swap_bytes);
    m_memSwapTotalGb = formatBytes(mem.total_swap_bytes);
    m_memUsagePct = (mem.total_ram_bytes > 0)
        ? (static_cast<qreal>(mem.used_ram_bytes) / static_cast<qreal>(mem.total_ram_bytes)) * 100.0
        : 0.0;
    appendRing(m_memHistory, m_memUsagePct);

    // 3. Disk I/O
    const auto& disk = snap.disk;
    m_diskReadBytesSec = disk.read_bytes_sec;
    m_diskWriteBytesSec = disk.write_bytes_sec;
    m_diskReadRate = formatRate(disk.read_bytes_sec);
    m_diskWriteRate = formatRate(disk.write_bytes_sec);
    m_diskIops = disk.iops;
    const qreal readKb = static_cast<qreal>(disk.read_bytes_sec) / 1024.0;
    const qreal writeKb = static_cast<qreal>(disk.write_bytes_sec) / 1024.0;
    appendRing(m_diskReadHistory, readKb);
    appendRing(m_diskWriteHistory, writeKb);
    appendRing(m_diskHistory, readKb + writeKb);

    // 4. Network I/O
    const auto& net = snap.network;
    m_netRxBytesSec = net.rx_bytes_sec;
    m_netTxBytesSec = net.tx_bytes_sec;
    m_netRxRate = formatRate(net.rx_bytes_sec);
    m_netTxRate = formatRate(net.tx_bytes_sec);
    m_netActiveInterface = net.active_interface.empty()
        ? QStringLiteral("Offline")
        : QString::fromStdString(net.active_interface);
    const qreal rxKb = static_cast<qreal>(net.rx_bytes_sec) / 1024.0;
    const qreal txKb = static_cast<qreal>(net.tx_bytes_sec) / 1024.0;
    appendRing(m_netRxHistory, rxKb);
    appendRing(m_netTxHistory, txKb);
    appendRing(m_netHistory, rxKb + txKb);

    // 5. GPU & Power
    const auto& gpu = snap.gpu;
    m_gpuVendor = QString::fromStdString(gpu.vendor);
    m_gpuDeviceName = QString::fromStdString(gpu.device_name);
    m_gpuDriver = QString::fromStdString(gpu.driver);
    m_gpuTelemetryStatus = QString::fromStdString(gpu.telemetry_status);
    m_gpuBusyPct = gpu.busy_percent;
    appendRing(m_gpuHistory, gpu.busy_percent >= 0.0f ? static_cast<qreal>(gpu.busy_percent) : 0.0);

    const auto& pwr = snap.power;
    m_hasBattery = pwr.has_battery;
    m_batteryPercent = pwr.battery_percent;
    m_powerStatus = QString::fromStdString(pwr.power_status);
    m_powerWatts = pwr.power_watts;

    emit telemetryChanged();
    emit historyChanged();

    if (hasProcs) {
        m_cachedProcesses = snap.processes;
        updateProcessesList();
    }
}

void MonitorBridge::updateProcessesList() {
    std::vector<ProcessInfo> filtered;
    filtered.reserve(m_cachedProcesses.size());

    QString qLower = m_searchQuery.toLower().trimmed();

    for (const auto& proc : m_cachedProcesses) {
        if (m_category == 1 && proc.uid != m_currentUid) {
            continue;
        }
        if (m_category == 2 && !proc.is_system_daemon) {
            continue;
        }

        if (!qLower.isEmpty()) {
            QString name = QString::fromStdString(proc.name).toLower();
            QString cmd = QString::fromStdString(proc.cmdline).toLower();
            QString pidStr = QString::number(proc.pid);
            if (!name.contains(qLower) && !cmd.contains(qLower) && !pidStr.contains(qLower)) {
                continue;
            }
        }

        filtered.push_back(proc);
    }

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

    const int newCount = static_cast<int>(filtered.size());
    const int oldCount = m_displayProcesses.size();
    bool pids_match = (newCount == oldCount);

    if (pids_match) {
        for (int i = 0; i < newCount; ++i) {
            QVariantMap existingMap = m_displayProcesses.at(i).toMap();
            if (existingMap.value(QStringLiteral("pid")).toInt() != filtered[i].pid) {
                pids_match = false;
                break;
            }
        }
    }

    auto formatDiskCol = [](const ProcessInfo& proc) -> QString {
        if (!proc.has_io_permission) {
            return QStringLiteral("—");
        }
        return formatRate(proc.read_bytes_sec + proc.write_bytes_sec);
    };

    if (pids_match) {
        for (int i = 0; i < newCount; ++i) {
            const auto& proc = filtered[i];
            QVariantMap map = m_displayProcesses.at(i).toMap();
            map[QStringLiteral("name")] = QString::fromStdString(proc.name);
            map[QStringLiteral("iconName")] = QString::fromStdString(proc.icon_name);
            map[QStringLiteral("threads")] = static_cast<int>(proc.threads);
            map[QStringLiteral("cpu")] = proc.cpu_percent;
            map[QStringLiteral("cpuStr")] = QString::asprintf("%.1f%%", static_cast<double>(proc.cpu_percent));
            map[QStringLiteral("rss")] = static_cast<qulonglong>(proc.rss_bytes);
            map[QStringLiteral("rssStr")] = formatBytes(proc.rss_bytes);
            map[QStringLiteral("diskStr")] = formatDiskCol(proc);
            map[QStringLiteral("state")] = QString::fromStdString(proc.state_str);
            map[QStringLiteral("energyImpact")] = QString::fromStdString(proc.energy_impact);
            m_displayProcesses[i] = map;
        }
    } else {
        m_displayProcesses.clear();
        m_displayProcesses.reserve(newCount);
        for (const auto& proc : filtered) {
            QVariantMap map;
            map[QStringLiteral("pid")] = proc.pid;
            map[QStringLiteral("ppid")] = proc.ppid;
            map[QStringLiteral("threads")] = static_cast<int>(proc.threads);
            map[QStringLiteral("name")] = QString::fromStdString(proc.name);
            map[QStringLiteral("cmdline")] = QString::fromStdString(proc.cmdline);
            map[QStringLiteral("iconName")] = QString::fromStdString(proc.icon_name);
            map[QStringLiteral("user")] = QString::fromStdString(proc.user);
            map[QStringLiteral("cpu")] = proc.cpu_percent;
            map[QStringLiteral("cpuStr")] = QString::asprintf("%.1f%%", static_cast<double>(proc.cpu_percent));
            map[QStringLiteral("rss")] = static_cast<qulonglong>(proc.rss_bytes);
            map[QStringLiteral("rssStr")] = formatBytes(proc.rss_bytes);
            map[QStringLiteral("diskStr")] = formatDiskCol(proc);
            map[QStringLiteral("state")] = QString::fromStdString(proc.state_str);
            map[QStringLiteral("energyImpact")] = QString::fromStdString(proc.energy_impact);
            map[QStringLiteral("isSystem")] = proc.is_system_daemon;
            map[QStringLiteral("isProtected")] = (proc.pid == 1) || tinexus::platform::PlatformServices::is_protected_pid(proc.pid);
            m_displayProcesses.append(map);
        }
    }

    emit processesChanged();
}

void MonitorBridge::applyServicesList(const std::vector<tinexus::platform::ServiceInfo>& svcs) {
    m_services.clear();
    m_cachedServices = svcs;
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
    QTimer::singleShot(180, this, &MonitorBridge::refresh);
    return ok;
}

QVariantMap MonitorBridge::inspectProcess(int pid) {
    if (pid <= 0) return {};
    selectProcess(pid);

    QVariantMap detail;
    for (const auto& proc : m_cachedProcesses) {
        if (proc.pid == pid) {
            detail[QStringLiteral("pid")] = proc.pid;
            detail[QStringLiteral("ppid")] = proc.ppid;
            detail[QStringLiteral("name")] = QString::fromStdString(proc.name);
            detail[QStringLiteral("cmdline")] = QString::fromStdString(proc.cmdline);
            detail[QStringLiteral("iconName")] = QString::fromStdString(proc.icon_name);
            detail[QStringLiteral("user")] = QString::fromStdString(proc.user);
            detail[QStringLiteral("threads")] = static_cast<int>(proc.threads);
            detail[QStringLiteral("state")] = QString::fromStdString(proc.state_str);
            detail[QStringLiteral("cpuStr")] = QString::asprintf("%.1f%%", static_cast<double>(proc.cpu_percent));
            detail[QStringLiteral("rssStr")] = formatBytes(proc.rss_bytes);
            detail[QStringLiteral("diskStr")] = proc.has_io_permission
                ? QStringLiteral("R: %1  W: %2").arg(formatRate(proc.read_bytes_sec), formatRate(proc.write_bytes_sec))
                : QStringLiteral("Restricted (non-owner)");
            detail[QStringLiteral("energyImpact")] = QString::fromStdString(proc.energy_impact);
            detail[QStringLiteral("isSystem")] = proc.is_system_daemon;
            detail[QStringLiteral("isProtected")] = (proc.pid == 1) || tinexus::platform::PlatformServices::is_protected_pid(proc.pid);
            break;
        }
    }

    int fdCount = -1;
    char fdPath[64];
    std::snprintf(fdPath, sizeof(fdPath), "/proc/%d/fd", pid);
    if (DIR* d = opendir(fdPath)) {
        fdCount = 0;
        while (struct dirent* e = readdir(d)) {
            if (e->d_name[0] != '.') ++fdCount;
        }
        closedir(d);
    }
    detail[QStringLiteral("openFds")] = (fdCount >= 0) ? QString::number(fdCount) : QStringLiteral("Restricted");

    m_inspectedProcess = detail;
    m_inspectModalOpen = !detail.isEmpty();
    emit inspectModalChanged();
    return detail;
}

void MonitorBridge::closeInspectModal() {
    if (m_inspectModalOpen) {
        m_inspectModalOpen = false;
        emit inspectModalChanged();
    }
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
                ProcessController::terminate_process(s.pid, false);
            } else {
                pid_t pid = fork();
                if (pid == 0) {
                    execlp(sname.c_str(), sname.c_str(), nullptr);
                    _exit(127);
                }
            }
            QTimer::singleShot(250, this, &MonitorBridge::refresh);
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
