// ============================================================================
// ShellBridge.cpp — C++20 QObject Bridge for tinexus-shell
// ============================================================================
#include "ShellBridge.hpp"
#include <common/AudioUtils.hpp>
#include <common/BacklightUtils.hpp>
#include <common/RuntimePaths.hpp>
#include <common/AppId.hpp>
#include <common/logger.hpp>

#include <QtCore/QProcess>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtNetwork/QLocalSocket>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnection>

#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace tinexus::shell {

ShellBridge::ShellBridge(QObject* parent)
    : QObject(parent)
{
    // Initialize hardware values from ALSA and sysfs
    m_volume = hardware::AudioUtils::get_volume_percent();
    m_soundMuted = hardware::AudioUtils::is_muted() || (m_volume == 0);
    m_brightness = hardware::BacklightUtils::get_brightness_percent();

    resolveLogoUrl();
    scanApplications();
    pollBattery();

    // Populate standard notifications for desktop notification flyout
    QVariantMap n1;
    n1[QStringLiteral("title")]   = QStringLiteral("System Update");
    n1[QStringLiteral("message")] = QStringLiteral("Tinexus Desktop v1.1.0 update is available.");
    n1[QStringLiteral("time")]    = QStringLiteral("10m ago");
    n1[QStringLiteral("appId")]   = QStringLiteral("tinexus-settings");

    QVariantMap n2;
    n2[QStringLiteral("title")]   = QStringLiteral("Battery");
    n2[QStringLiteral("message")] = QStringLiteral("Power adapter connected.");
    n2[QStringLiteral("time")]    = QStringLiteral("25m ago");
    n2[QStringLiteral("appId")]   = QStringLiteral("tinexus-monitor");

    m_notifications.append(n1);
    m_notifications.append(n2);

    updateClock();
    connect(&m_clockTimer, &QTimer::timeout, this, &ShellBridge::updateClock);
    m_clockTimer.start(1000);
}

void ShellBridge::resolveLogoUrl() {
    const QStringList candidates = {
        QStringLiteral("/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png"),
        QStringLiteral("/usr/share/tinexus/tinexus-logo.png"),
        QStringLiteral("/usr/share/pixmaps/tinexus-logo.png"),
        QStringLiteral("assets/logo/tinexus-logo-32.png"),
        QStringLiteral("../assets/logo/tinexus-logo-32.png"),
        QStringLiteral("../../assets/logo/tinexus-logo-32.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/logo/tinexus-logo-32.png"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/assets/logo/tinexus-logo-32.png"),
        QStringLiteral("assets/logo/tinexus-logo.svg"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/assets/logo/tinexus-logo.svg")
    };

    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            m_logoUrl = QUrl::fromLocalFile(QFileInfo(cand).absoluteFilePath()).toString();
            break;
        }
    }
}

void ShellBridge::scanApplications() {
    m_applicationsList.clear();
    const std::vector<std::string> dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    for (const auto& d : dirs) {
        std::error_code ec;
        if (!fs::exists(d, ec)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(d, ec)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".desktop") continue;
                std::ifstream f(entry.path());
                if (!f.is_open()) continue;

                std::string line;
                std::string name;
                std::string exec;
                std::string icon;
                std::string comment;
                bool no_display = false;
                bool is_desktop_entry = false;

                while (std::getline(f, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line == "[Desktop Entry]") { is_desktop_entry = true; continue; }
                    if (!line.empty() && line[0] == '[' && is_desktop_entry) break;
                    if (!is_desktop_entry) continue;

                    if (line.rfind("Name=", 0) == 0 && name.empty()) {
                        name = line.substr(5);
                    } else if (line.rfind("Exec=", 0) == 0 && exec.empty()) {
                        exec = line.substr(5);
                    } else if (line.rfind("Icon=", 0) == 0 && icon.empty()) {
                        icon = line.substr(5);
                    } else if (line.rfind("Comment=", 0) == 0 && comment.empty()) {
                        comment = line.substr(8);
                    } else if (line == "NoDisplay=true") {
                        no_display = true;
                    }
                }

                if (no_display || name.empty() || exec.empty()) continue;

                // Clean exec arguments (%u, %U, %f, %F, etc.)
                size_t pct = exec.find('%');
                while (pct != std::string::npos) {
                    if (pct + 1 < exec.size()) {
                        exec.erase(pct, 2);
                    } else {
                        exec.erase(pct, 1);
                    }
                    pct = exec.find('%');
                }
                while (!exec.empty() && exec.back() == ' ') exec.pop_back();

                // Avoid duplicate names
                bool duplicate = false;
                for (const auto& existing : m_applicationsList) {
                    if (existing.toMap().value(QStringLiteral("name")).toString() == QString::fromStdString(name)) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;

                QVariantMap item;
                item[QStringLiteral("name")] = QString::fromStdString(name);
                item[QStringLiteral("exec")] = QString::fromStdString(exec);
                item[QStringLiteral("icon")] = QString::fromStdString(icon);
                item[QStringLiteral("comment")] = QString::fromStdString(comment);
                m_applicationsList.append(item);
            }
        } catch (...) {}
    }

    // Ensure Firefox is always present if installed
    bool has_firefox = false;
    for (const auto& existing : m_applicationsList) {
        if (existing.toMap().value(QStringLiteral("name")).toString().contains(QStringLiteral("Firefox"), Qt::CaseInsensitive)) {
            has_firefox = true;
            break;
        }
    }
    if (!has_firefox && (fs::exists("/usr/bin/firefox") || fs::exists("/usr/share/applications/firefox.desktop"))) {
        QVariantMap ff;
        ff[QStringLiteral("name")] = QStringLiteral("Firefox");
        ff[QStringLiteral("exec")] = QStringLiteral("env MOZ_ENABLE_WAYLAND=1 firefox");
        ff[QStringLiteral("icon")] = QStringLiteral("firefox");
        ff[QStringLiteral("comment")] = QStringLiteral("Mozilla Firefox Web Browser");
        m_applicationsList.append(ff);
    }

    // Alphabetically sort applications for quick, intuitive navigation
    std::sort(m_applicationsList.begin(), m_applicationsList.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("name")).toString().toLower() <
               b.toMap().value(QStringLiteral("name")).toString().toLower();
    });

    emit applicationsListChanged();
}

void ShellBridge::pollBattery() {
    if (!fs::exists("/sys/class/power_supply")) {
        if (m_batteryPercent != -1) {
            m_batteryPercent = -1;
            emit batteryChanged();
        }
        return;
    }

    double total_cap = 0.0;
    int battery_count = 0;

    try {
        for (const auto& entry : fs::directory_iterator("/sys/class/power_supply")) {
            if (!entry.is_directory()) continue;
            std::string type;
            fs::path type_file = entry.path() / "type";
            if (fs::exists(type_file)) {
                std::ifstream tf(type_file);
                tf >> type;
            }

            std::string name = entry.path().filename().string();
            bool is_battery = (type == "Battery" || name.rfind("BAT", 0) == 0 || name.find("battery") != std::string::npos);
            if (!is_battery) continue;

            fs::path present_file = entry.path() / "present";
            if (fs::exists(present_file)) {
                std::ifstream pf(present_file);
                int present = 1;
                if (pf >> present && present == 0) continue;
            }

            fs::path cap_file = entry.path() / "capacity";
            if (fs::exists(cap_file)) {
                std::ifstream cf(cap_file);
                int cap = -1;
                if (cf >> cap && cap >= 0 && cap <= 100) {
                    total_cap += cap;
                    battery_count++;
                }
            }
        }
    } catch (...) {}

    int new_pct = (battery_count > 0) ? static_cast<int>(total_cap / battery_count) : -1;
    if (m_batteryPercent != new_pct) {
        m_batteryPercent = new_pct;
        emit batteryChanged();
    }
}

void ShellBridge::syncAudioState() {
    int cur_vol = hardware::AudioUtils::get_volume_percent();
    bool cur_muted = hardware::AudioUtils::is_muted() || (cur_vol == 0);
    if (m_volume != cur_vol || m_soundMuted != cur_muted) {
        m_volume = cur_vol;
        m_soundMuted = cur_muted;
        emit volumeChanged();
    }
}

void ShellBridge::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    const QString newTime = now.toString(QStringLiteral("h:mm AP"));
    const QString newDate = now.toString(QStringLiteral("ddd, MMM d"));

    if (newTime != m_currentTime || newDate != m_currentDate) {
        m_currentTime = newTime;
        m_currentDate = newDate;
        emit timeChanged();
    }

    pollBattery();
    syncAudioState();
}

void ShellBridge::setVolume(int vol) {
    vol = std::clamp(vol, 0, 100);
    hardware::AudioUtils::set_volume_percent(vol, /*persist=*/true);
    m_volume = hardware::AudioUtils::get_volume_percent();
    m_soundMuted = hardware::AudioUtils::is_muted() || (m_volume == 0);
    emit volumeChanged();
}

void ShellBridge::setSoundMuted(bool muted) {
    if (hardware::AudioUtils::is_muted() != muted) {
        hardware::AudioUtils::toggle_mute(/*persist=*/true);
    }
    m_soundMuted = hardware::AudioUtils::is_muted();
    emit volumeChanged();
}

void ShellBridge::setBrightness(int bri) {
    bri = std::clamp(bri, 0, 100);
    hardware::BacklightUtils::set_brightness_percent(bri, /*persist=*/true);
    m_brightness = hardware::BacklightUtils::get_brightness_percent();
    emit brightnessChanged();
}

void ShellBridge::setActiveAppName(const QString& name) {
    if (m_activeAppName != name) {
        m_activeAppName = name;
        emit activeAppChanged();
    }
}

void ShellBridge::closeAllFlyouts() {
    bool changed = m_logoMenuOpen || m_appMenuOpen || m_calendarOpen || m_notificationsOpen ||
                   m_volumeFlyoutOpen || m_brightnessFlyoutOpen || m_rebootConfirmationOpen || m_shutdownConfirmationOpen;

    m_logoMenuOpen             = false;
    m_appMenuOpen              = false;
    m_calendarOpen             = false;
    m_notificationsOpen        = false;
    m_volumeFlyoutOpen         = false;
    m_brightnessFlyoutOpen     = false;
    m_rebootConfirmationOpen   = false;
    m_shutdownConfirmationOpen = false;

    if (changed) {
        emit flyoutStateChanged();
    }
}

void ShellBridge::setLogoMenuOpen(bool open) {
    if (open) closeAllFlyouts();
    m_logoMenuOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setAppMenuOpen(bool open) {
    if (open) closeAllFlyouts();
    m_appMenuOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setCalendarOpen(bool open) {
    if (open) closeAllFlyouts();
    m_calendarOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setNotificationsOpen(bool open) {
    if (open) closeAllFlyouts();
    m_notificationsOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setVolumeFlyoutOpen(bool open) {
    if (open) {
        closeAllFlyouts();
        int cur_vol = hardware::AudioUtils::get_volume_percent();
        bool cur_muted = hardware::AudioUtils::is_muted() || (cur_vol == 0);
        if (m_volume != cur_vol || m_soundMuted != cur_muted) {
            m_volume = cur_vol;
            m_soundMuted = cur_muted;
            emit volumeChanged();
        }
    }
    m_volumeFlyoutOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setBrightnessFlyoutOpen(bool open) {
    if (open) closeAllFlyouts();
    m_brightnessFlyoutOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setRebootConfirmationOpen(bool open) {
    if (open) closeAllFlyouts();
    m_rebootConfirmationOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::setShutdownConfirmationOpen(bool open) {
    if (open) closeAllFlyouts();
    m_shutdownConfirmationOpen = open;
    emit flyoutStateChanged();
}

void ShellBridge::toggleLogoMenu() {
    bool next = !m_logoMenuOpen;
    closeAllFlyouts();
    m_logoMenuOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleAppMenu() {
    bool next = !m_appMenuOpen;
    closeAllFlyouts();
    m_appMenuOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleCalendar() {
    bool next = !m_calendarOpen;
    closeAllFlyouts();
    m_calendarOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleNotifications() {
    bool next = !m_notificationsOpen;
    closeAllFlyouts();
    m_notificationsOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleVolume() {
    bool next = !m_volumeFlyoutOpen;
    closeAllFlyouts();
    m_volumeFlyoutOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::toggleBrightness() {
    bool next = !m_brightnessFlyoutOpen;
    closeAllFlyouts();
    m_brightnessFlyoutOpen = next;
    emit flyoutStateChanged();
}

void ShellBridge::clearNotifications() {
    m_notifications.clear();
    emit notificationsChanged();
}

void ShellBridge::launchApp(const QString& execCmd) {
    tinexus::log::info("[Shell] launchApp: {}", execCmd.toStdString());
    QStringList parts = QProcess::splitCommand(execCmd);
    if (parts.isEmpty()) return;
    QString prog = parts.takeFirst();
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    proc.setProcessEnvironment(env);
    proc.setProgram(prog);
    proc.setArguments(parts);
    proc.startDetached();
}

void ShellBridge::openWifiSettings() {
    tinexus::log::info("[Shell] Wi-Fi icon clicked -> launching tinexus-settings-ui");
    launchApp(QStringLiteral("tinexus-settings-ui"));
    closeAllFlyouts();
}

void ShellBridge::onNotchCenterClicked() {
    tinexus::log::info("[Shell] Notch center clicked -> toggling launcher");
    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("tinexus-launcher-single"));
    if (socket.waitForConnected(250)) {
        socket.write("toggle\n");
        socket.waitForBytesWritten(250);
        socket.flush();
    } else {
        launchApp(QStringLiteral("tinexus-launcher"));
    }
}

void ShellBridge::requestReboot() {
    closeAllFlyouts();
    m_rebootConfirmationOpen = true;
    emit flyoutStateChanged();
}

void ShellBridge::requestShutdown() {
    closeAllFlyouts();
    m_shutdownConfirmationOpen = true;
    emit flyoutStateChanged();
}

void ShellBridge::powerReboot() {
    tinexus::log::info("[Shell] Executing system Reboot via logind D-Bus");
    closeAllFlyouts();
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        login1.call(QStringLiteral("Reboot"), true);
    } else {
        sync();
        ::reboot(RB_AUTOBOOT);
    }
}

void ShellBridge::powerShutdown() {
    tinexus::log::info("[Shell] Executing system PowerOff via logind D-Bus");
    closeAllFlyouts();
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        login1.call(QStringLiteral("PowerOff"), true);
    } else {
        sync();
        ::reboot(RB_POWER_OFF);
    }
}

void ShellBridge::powerSleep() {
    tinexus::log::info("[Shell] Executing system Suspend via logind D-Bus");
    closeAllFlyouts();
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        login1.call(QStringLiteral("Suspend"), true);
    }
}

} // namespace tinexus::shell

#include "moc_ShellBridge.cpp"
