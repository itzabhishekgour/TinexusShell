#include "AboutBridge.hpp"
#include <common/DisplayUtils.hpp>
#include <common/PlatformServices.hpp>
#include <common/logger.hpp>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace tinexus::about {

AboutBridge::AboutBridge(QObject* parent)
    : QObject(parent)
{
    refresh();
}

void AboutBridge::refresh() {
    readSystemInfo();
    readDisplayInfo();
    readStorageInfo();
    readServicesInfo();
}

void AboutBridge::readSystemInfo() {
    m_osTitle = QStringLiteral("Tinexus Desktop");
    m_osVersion = QStringLiteral("Version 1.0 (Architecture Freeze - LTS)");
    m_platformInfo = QStringLiteral("Wayland Native / Pure C++20");

    // 1. Kernel Version & Architecture
    struct utsname un;
    if (uname(&un) == 0) {
        std::string rel = un.release;
        auto ms_pos = rel.find("-microsoft");
        if (ms_pos != std::string::npos) {
            rel = rel.substr(0, ms_pos);
        }
        m_kernelVersion = QStringLiteral("%1 %2 (%3)")
            .arg(QString::fromUtf8(un.sysname))
            .arg(QString::fromUtf8(rel.c_str()))
            .arg(QString::fromUtf8(un.machine));
    } else {
        m_kernelVersion = QStringLiteral("Linux 6.18.33 (x86_64)");
    }

    // 2. Memory
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        uint64_t total_mb = (si.totalram * si.mem_unit) / (1024 * 1024);
        uint64_t free_mb  = (si.freeram  * si.mem_unit) / (1024 * 1024);
        uint64_t used_mb  = (total_mb > free_mb) ? (total_mb - free_mb) : 0;
        double used_gb = static_cast<double>(used_mb) / 1024.0;
        double total_gb = static_cast<double>(total_mb) / 1024.0;
        int pct = (total_mb > 0) ? static_cast<int>((used_mb * 100) / total_mb) : 0;

        m_memInfo = QStringLiteral("%1 GB / %2 GB (%3% used)")
            .arg(used_gb, 0, 'f', 1)
            .arg(total_gb, 0, 'f', 1)
            .arg(pct);
    } else {
        m_memInfo = QStringLiteral("8.0 GB DDR4 Memory");
    }

    // 3. CPU
    std::string cpu_model;
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0 || line.rfind("Hardware", 0) == 0) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    cpu_model = line.substr(colon + 2);
                    break;
                }
            }
        }
    }
    if (cpu_model.empty()) {
        cpu_model = "Intel Core Processor";
    }

    auto remove_all = [](std::string& s, const std::string& target) {
        size_t pos = 0;
        while ((pos = s.find(target, pos)) != std::string::npos) {
            s.erase(pos, target.length());
        }
    };
    remove_all(cpu_model, "(R)");
    remove_all(cpu_model, "(TM)");
    remove_all(cpu_model, "CPU ");
    while (cpu_model.find("  ") != std::string::npos) {
        auto pos = cpu_model.find("  ");
        cpu_model.replace(pos, 2, " ");
    }
    m_cpuModel = QString::fromUtf8(cpu_model.c_str());

    // 4. Compositor / Graphics
    m_graphicsInfo = QString::fromUtf8(
        tinexus::hardware::DisplayUtils::get_compositor_version_string().c_str()
    );

    emit systemInfoChanged();
}

void AboutBridge::readDisplayInfo() {
    auto disp = tinexus::hardware::DisplayUtils::get_primary_display();
    m_displayName = QString::fromUtf8(disp.connector_name.c_str()) + 
        (disp.connected ? QStringLiteral(" (Connected)") : QStringLiteral(" (Built-in Display)"));

    std::string res_fmt = disp.resolution;
    auto x_p = res_fmt.find('x');
    if (x_p != std::string::npos) res_fmt.replace(x_p, 1, " × ");
    m_displayResolution = QString::fromUtf8(res_fmt.c_str()) + QStringLiteral(" (Native)");
    m_displayRefreshRate = QString::fromUtf8(disp.refresh_rate.c_str()) + QStringLiteral(" (Hardware VSync)");
    m_displayScale = QStringLiteral("100% (Native 1:1 Pixel Grid)");
    m_displayColorFormat = QStringLiteral("32-bit ARGB8888 (sRGB D65)");
    m_displayRenderer = QString::fromUtf8(
        tinexus::hardware::DisplayUtils::get_renderer_backend_string().c_str()
    );

    emit displayInfoChanged();
}

void AboutBridge::readStorageInfo() {
    m_storageMount = QStringLiteral("/");
    m_storageDevice = QStringLiteral("Root NVMe SSD");
    m_storageFsType = QStringLiteral("ext4 / OverlayFS");

    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string dev, mnt, fstype;
        while (mounts >> dev >> mnt >> fstype) {
            std::string dummy;
            std::getline(mounts, dummy);
            if (mnt == "/") {
                m_storageDevice = QString::fromUtf8(dev.c_str());
                m_storageFsType = QString::fromUtf8(fstype.c_str());
                break;
            }
        }
    }
    m_rootDevice = m_storageDevice;

    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        uint64_t total = (sv.f_blocks * sv.f_frsize) / (1024ULL * 1024ULL * 1024ULL);
        uint64_t free  = (sv.f_bfree  * sv.f_frsize) / (1024ULL * 1024ULL * 1024ULL);
        uint64_t used  = (total > free) ? (total - free) : 0;
        if (total == 0) { total = 64; used = 16; free = 48; }
        m_storageTotalGb = static_cast<qreal>(total);
        m_storageUsedGb = static_cast<qreal>(used);
        m_storageFreeGb = static_cast<qreal>(free);
        m_storageUsedPct = m_storageTotalGb > 0 ? (m_storageUsedGb / m_storageTotalGb) : 0.25;
    } else {
        m_storageTotalGb = 64.0;
        m_storageUsedGb = 16.0;
        m_storageFreeGb = 48.0;
        m_storageUsedPct = 0.25;
    }

    emit storageInfoChanged();
}

void AboutBridge::readServicesInfo() {
    m_services.clear();
    auto svcs = tinexus::platform::PlatformServices::query_supervised_services();
    for (const auto& s : svcs) {
        QVariantMap map;
        map[QStringLiteral("name")] = QString::fromUtf8(s.name.c_str());
        map[QStringLiteral("role")] = QString::fromUtf8(s.role.c_str());
        map[QStringLiteral("active")] = s.active;
        map[QStringLiteral("status")] = s.active ? QStringLiteral("ACTIVE") : QStringLiteral("STANDBY");
        map[QStringLiteral("pid")] = s.active ? QStringLiteral("PID %1").arg(s.pid) : QStringLiteral("Supervised");
        m_services.append(map);
    }
    emit servicesChanged();
}

void AboutBridge::launchSystemReport() {
    log::info("[About] Launching System Monitor (System Report)...");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("tinexus-monitor", "tinexus-monitor", nullptr);
        _exit(127);
    }
}

void AboutBridge::launchSoftwareUpdate() {
    log::info("[About] Launching Package Manager (Software Update)...");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("tinexus-pkg", "tinexus-pkg", nullptr);
        _exit(127);
    }
}

} // namespace tinexus::about
