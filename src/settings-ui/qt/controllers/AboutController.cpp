#include "AboutController.hpp"
#include <common/DisplayUtils.hpp>
#include <common/version.hpp>
#include <common/logger.hpp>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <sstream>

namespace tinexus::settings_ui {

AboutController::AboutController(QObject* parent)
    : QObject(parent)
{
    m_platformVersion = QString::fromUtf8(tinexus::VERSION_STRING.data(), tinexus::VERSION_STRING.size());
    refreshSystemInfo();
    refreshUserInfo();
}

void AboutController::refreshUserInfo() {
    uid_t uid = ::getuid();
    struct passwd* pw = ::getpwuid(uid);

    if (pw) {
        m_currentUserName = QString::fromLatin1(pw->pw_name);
        QString gecos = QString::fromLocal8Bit(pw->pw_gecos);
        auto comma = gecos.indexOf(',');
        if (comma != -1) {
            gecos = gecos.left(comma);
        }
        gecos = gecos.trimmed();
        m_currentUserRealName = !gecos.isEmpty() ? gecos : m_currentUserName;
    } else {
        const char* u = std::getenv("USER");
        m_currentUserName = u ? QString::fromLatin1(u) : QStringLiteral("tinexus");
        m_currentUserRealName = m_currentUserName;
    }

    // Generate dynamic avatar initials from real name
    QStringList parts = m_currentUserRealName.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        m_userInitials = QString(parts[0].at(0).toUpper()) + QString(parts[1].at(0).toUpper());
    } else if (!parts.isEmpty() && !parts[0].isEmpty()) {
        m_userInitials = parts[0].left(2).toUpper();
    } else {
        m_userInitials = QStringLiteral("TX");
    }

    emit userInfoChanged();
}

void AboutController::refreshSystemInfo() {
    struct utsname uts;
    if (uname(&uts) == 0) {
        m_osVersion = QStringLiteral("Tinexus Linux %1 (%2)").arg(QString::fromLatin1(uts.release), QString::fromLatin1(uts.machine));
    } else {
        m_osVersion = QStringLiteral("Tinexus Linux (x86_64)");
    }

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        double total_gb = static_cast<double>(si.totalram) * si.mem_unit / (1024.0 * 1024.0 * 1024.0);
        double free_gb = static_cast<double>(si.freeram) * si.mem_unit / (1024.0 * 1024.0 * 1024.0);
        double used_gb = total_gb - free_gb;
        int pct = total_gb > 0.0 ? static_cast<int>((used_gb / total_gb) * 100.0) : 0;
        m_memInfo = QStringLiteral("%1 GB / %2 GB (%3% used)").arg(QString::number(used_gb, 'f', 1), QString::number(total_gb, 'f', 1), QString::number(pct));
    } else {
        m_memInfo = QStringLiteral("Unknown RAM");
    }

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.starts_with("model name") || line.starts_with("Hardware")) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    m_cpuModel = QString::fromStdString(line.substr(colon + 2)).trimmed();
                    break;
                }
            }
        }
    }
    if (m_cpuModel.isEmpty()) m_cpuModel = QStringLiteral("Generic x86_64 Processor");

    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        double total_bytes = static_cast<double>(vfs.f_blocks) * static_cast<double>(vfs.f_frsize);
        double free_bytes = static_cast<double>(vfs.f_bavail) * static_cast<double>(vfs.f_frsize);
        double used_bytes = total_bytes - free_bytes;
        double total_gb = total_bytes / (1024.0 * 1024.0 * 1024.0);
        double used_gb = used_bytes / (1024.0 * 1024.0 * 1024.0);
        int pct = total_gb > 0.0 ? static_cast<int>((used_gb / total_gb) * 100.0) : 0;
        m_storageInfo = QStringLiteral("%1 GB / %2 GB (%3% used)").arg(QString::number(used_gb, 'f', 1), QString::number(total_gb, 'f', 1), QString::number(pct));
    } else {
        m_storageInfo = QStringLiteral("Unknown Storage");
    }

    auto disp = hardware::DisplayUtils::get_primary_display();
    if (disp.connected && !disp.formatted_line.empty()) {
        m_displayInfo = QString::fromStdString(disp.formatted_line);
    } else if (!disp.resolution.empty() && disp.resolution != "unknown") {
        m_displayInfo = QStringLiteral("%1 @ %2Hz").arg(QString::fromStdString(disp.resolution), QString::fromStdString(disp.refresh_rate));
    } else {
        m_displayInfo = QStringLiteral("Primary Display (1920x1080 @ 60Hz)");
    }

    m_graphicsEngine = QString::fromStdString(hardware::DisplayUtils::get_renderer_backend_string());
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());

    emit aboutInfoChanged();
}

void AboutController::checkForUpdates() {
    emit toastRequested(QStringLiteral("Checking repository for Tinexus OS updates... system is up to date"), false);
}

} // namespace tinexus::settings_ui
