#include "SettingsBridge.hpp"
#include "settings/WifiManager.hpp"
#include <common/NetUtils.hpp>
#include <common/AudioUtils.hpp>
#include <common/BacklightUtils.hpp>
#include <common/DisplayUtils.hpp>
#include <common/version.hpp>
#include <common/logger.hpp>
#include <guard/crypto_validator.hpp>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QProcess>

namespace tinexus::settings_ui {

namespace {

struct AccentDef {
    const char* hex;
    const char* name;
};

const AccentDef ACCENT_TABLE[] = {
    {"#00c3ff", "Electric Cyan"},
    {"#007aff", "macOS Blue"},
    {"#5856d6", "Deep Violet"},
    {"#ff2d55", "Hot Pink"},
    {"#ff9500", "Vibrant Orange"},
    {"#34c759", "Mint Green"}
};
constexpr int ACCENT_COUNT = sizeof(ACCENT_TABLE) / sizeof(ACCENT_TABLE[0]);

} // anonymous namespace

SettingsBridge::SettingsBridge(QObject* parent)
    : QObject(parent)
{
    m_platformVersion = QString::fromUtf8(tinexus::VERSION_STRING.data(), tinexus::VERSION_STRING.size());
    readSystemInfo();
    scanWallpapers();
    refreshNetworkInterfaces();
    rescanApps();
    loadConfig();

    // Sync hardware initial values
    m_brightness = hardware::BacklightUtils::get_brightness_percent();
    m_volume = hardware::AudioUtils::get_volume_percent();
    m_muted = hardware::AudioUtils::is_muted();
}

SettingsBridge::~SettingsBridge() = default;

QString SettingsBridge::accentColor() const {
    int idx = std::clamp(m_accentIndex, 0, ACCENT_COUNT - 1);
    return QString::fromLatin1(ACCENT_TABLE[idx].hex);
}

bool SettingsBridge::wifiEnabled() const {
    return WifiManager::instance().is_wifi_enabled();
}

bool SettingsBridge::isScanning() const {
    return WifiManager::instance().is_scanning();
}

bool SettingsBridge::isConnecting() const {
    return WifiManager::instance().is_connecting();
}

QString SettingsBridge::connectedSsid() const {
    return QString::fromStdString(WifiManager::instance().get_connected_ssid());
}

QString SettingsBridge::connectingSsid() const {
    return QString::fromStdString(WifiManager::instance().get_connecting_ssid());
}

QString SettingsBridge::ipAddress() const {
    return QString::fromStdString(WifiManager::instance().get_ip_address());
}

QString SettingsBridge::statusMessage() const {
    return QString::fromStdString(WifiManager::instance().get_status_message());
}

int SettingsBridge::connectedSignalBars() const {
    return WifiManager::instance().get_connected_signal_bars();
}

QVariantList SettingsBridge::wifiNetworks() const {
    QVariantList list;
    auto nets = WifiManager::instance().get_networks();
    for (const auto& n : nets) {
        QVariantMap map;
        map["ssid"] = QString::fromStdString(n.ssid);
        map["bssid"] = QString::fromStdString(n.bssid);
        map["bars"] = n.signal_bars;
        map["signalDbm"] = n.signal_dbm;
        map["frequencyMhz"] = n.frequency_mhz;
        map["secured"] = n.is_secured;
        map["security"] = QString::fromStdString(n.security_str);
        map["connected"] = n.is_connected;
        list.append(map);
    }
    return list;
}

void SettingsBridge::selectPage(int page) {
    if (m_currentPage != page) {
        m_currentPage = page;
        emit currentPageChanged();
    }
}

void SettingsBridge::setDisplayScaleIndex(int index) {
    if (m_displayScaleIndex != index) {
        m_displayScaleIndex = std::clamp(index, 0, 3);
        saveConfig();
        emit displayScaleIndexChanged();
        emit toastNotification(QStringLiteral("Display scale updated to %1%").arg(100 + m_displayScaleIndex * 25), false);
    }
}

void SettingsBridge::setBrightness(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (m_brightness != percent) {
        m_brightness = percent;
        hardware::BacklightUtils::set_brightness_percent(m_brightness, /*persist=*/false, /*throttle=*/true);
        emit brightnessChanged();
    }
}

void SettingsBridge::setNightLight(bool enabled) {
    if (m_nightLight != enabled) {
        m_nightLight = enabled;
        saveConfig();
        emit nightLightChanged();
        emit toastNotification(m_nightLight ? QStringLiteral("Night Light enabled") : QStringLiteral("Night Light disabled"), false);
    }
}

void SettingsBridge::setVrrEnabled(bool enabled) {
    if (m_vrrEnabled != enabled) {
        m_vrrEnabled = enabled;
        saveConfig();
        emit vrrEnabledChanged();
        emit toastNotification(m_vrrEnabled ? QStringLiteral("Adaptive Sync / VRR enabled") : QStringLiteral("Adaptive Sync / VRR disabled"), false);
    }
}

void SettingsBridge::setVolume(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (m_volume != percent) {
        m_volume = percent;
        hardware::AudioUtils::set_volume_percent(m_volume, /*persist=*/false, /*throttle=*/true);
        emit volumeChanged();
        if (m_muted != hardware::AudioUtils::is_muted()) {
            m_muted = hardware::AudioUtils::is_muted();
            emit mutedChanged();
        }
    }
}

void SettingsBridge::setMuted(bool muted) {
    if (m_muted != muted) {
        m_muted = muted;
        if (hardware::AudioUtils::is_muted() != m_muted) {
            hardware::AudioUtils::toggle_mute(/*persist=*/true);
        }
        emit mutedChanged();
        emit toastNotification(m_muted ? QStringLiteral("Audio output muted") : QStringLiteral("Audio output unmuted"), false);
    }
}

void SettingsBridge::playTestSound() {
    hardware::AudioUtils::play_chime();
    emit toastNotification(QStringLiteral("Playing system test sound..."), false);
}

void SettingsBridge::setAccentIndex(int index) {
    if (index >= 0 && index < ACCENT_COUNT && m_accentIndex != index) {
        m_accentIndex = index;
        saveConfig();
        emit accentIndexChanged();
        emit accentColorChanged();
        emit toastNotification(QStringLiteral("Accent theme set to %1").arg(QString::fromLatin1(ACCENT_TABLE[m_accentIndex].name)), false);
    }
}

void SettingsBridge::setThemeMode(const QString& mode) {
    if (m_themeMode != mode) {
        m_themeMode = mode;
        saveConfig();
        emit themeModeChanged();
        emit toastNotification(QStringLiteral("Theme switched to %1").arg(m_themeMode), false);
    }
}

void SettingsBridge::setSelectedWallpaperIndex(int index) {
    if (index >= 0 && index < m_wallpapers.size()) {
        m_selectedWallpaperIndex = index;
        QString path = m_wallpapers[index].toMap().value(QStringLiteral("path")).toString();

        // Write current wallpaper path to runtime locations for tinexus-wallpaper
        QString runDir = QStringLiteral("/run/user/%1/tinexus").arg(getuid());
        QDir().mkpath(runDir);
        QFile curWall(runDir + QStringLiteral("/current_wallpaper"));
        if (curWall.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            curWall.write(path.toUtf8());
            curWall.close();
        }
        QFile tmpWall(QStringLiteral("/tmp/current_wallpaper"));
        if (tmpWall.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            tmpWall.write(path.toUtf8());
            tmpWall.close();
        }

        saveConfig();
        emit selectedWallpaperIndexChanged();

        // Signal wallpaper daemon to reload immediately
        QProcess::startDetached(QStringLiteral("pkill"), {QStringLiteral("-USR1"), QStringLiteral("-f"), QStringLiteral("tinexus-wallpaper")});

        emit toastNotification(QStringLiteral("Wallpaper applied: %1").arg(m_wallpapers[index].toMap().value(QStringLiteral("name")).toString()), false);
    }
}

void SettingsBridge::setWifiEnabled(bool enabled) {
    WifiManager::instance().set_wifi_enabled(enabled);
    emit wifiEnabledChanged();
    emit wifiNetworksChanged();
    emit toastNotification(enabled ? QStringLiteral("Wi-Fi subsystem enabled") : QStringLiteral("Wi-Fi subsystem disabled"), false);
}

void SettingsBridge::triggerWifiScan() {
    WifiManager::instance().trigger_scan();
    emit isScanningChanged();
    emit toastNotification(QStringLiteral("Scanning for available Wi-Fi networks..."), false);
}

void SettingsBridge::connectWifi(const QString& ssid, const QString& password) {
    WifiManager::instance().connect(ssid.toStdString(), password.toStdString());
    emit isConnectingChanged();
    emit connectingSsidChanged();
    emit toastNotification(QStringLiteral("Connecting to \"%1\"...").arg(ssid), false);
}

void SettingsBridge::disconnectWifi() {
    WifiManager::instance().disconnect();
    emit connectedSsidChanged();
    emit ipAddressChanged();
    emit toastNotification(QStringLiteral("Disconnected from Wi-Fi"), false);
}

void SettingsBridge::refreshNetworkInterfaces() {
    m_networkInterfaces.clear();
    auto phys = tinexus::net::get_physical_interfaces("/sys/class/net");
    for (const auto& p : phys) {
        QVariantMap iface;
        iface["name"] = QString::fromStdString(p.name);
        iface["ip4"] = QString::fromStdString(p.ip4_addr);
        iface["state"] = QString::fromStdString(p.operstate);
        iface["rx"] = static_cast<qulonglong>(p.rx_mb);
        iface["tx"] = static_cast<qulonglong>(p.tx_mb);
        m_networkInterfaces.append(iface);
    }
    emit networkInterfacesChanged();
}

void SettingsBridge::setScreenTimeoutMin(int min) {
    min = std::clamp(min, 1, 60);
    if (m_screenTimeoutMin != min) {
        m_screenTimeoutMin = min;
        saveConfig();
        emit screenTimeoutMinChanged();
    }
}

void SettingsBridge::setSleepAfterMin(int min) {
    min = std::clamp(min, 5, 120);
    if (m_sleepAfterMin != min) {
        m_sleepAfterMin = min;
        saveConfig();
        emit sleepAfterMinChanged();
    }
}

void SettingsBridge::setPowerProfileIndex(int index) {
    index = std::clamp(index, 0, 2);
    if (m_powerProfileIndex != index) {
        m_powerProfileIndex = index;
        saveConfig();
        emit powerProfileIndexChanged();
        const char* profiles[] = {"Power Saver", "Balanced", "Performance"};
        emit toastNotification(QStringLiteral("Power profile set to %1").arg(QString::fromLatin1(profiles[m_powerProfileIndex])), false);
    }
}

void SettingsBridge::setLockOnSleep(bool enabled) {
    if (m_lockOnSleep != enabled) {
        m_lockOnSleep = enabled;
        saveConfig();
        emit lockOnSleepChanged();
    }
}

void SettingsBridge::setPamAuth(bool enabled) {
    if (m_pamAuth != enabled) {
        m_pamAuth = enabled;
        saveConfig();
        emit pamAuthChanged();
    }
}

void SettingsBridge::setClipboardHistorySize(int size) {
    size = std::clamp(size, 10, 200);
    if (m_clipboardHistorySize != size) {
        m_clipboardHistorySize = size;
        saveConfig();
        emit clipboardHistorySizeChanged();
    }
}

void SettingsBridge::sessionLock() {
    emit toastNotification(QStringLiteral("Requesting desktop session lock..."), false);
}

void SettingsBridge::sessionSuspend() {
    emit toastNotification(QStringLiteral("Requesting system suspend..."), false);
}

void SettingsBridge::sessionReboot() {
    emit toastNotification(QStringLiteral("System reboot initiated"), false);
    sync();
    ::reboot(RB_AUTOBOOT);
}

void SettingsBridge::sessionShutdown() {
    emit toastNotification(QStringLiteral("System shutdown initiated"), false);
    sync();
    ::reboot(RB_POWER_OFF);
}

void SettingsBridge::rescanApps() {
    m_unverifiedApps.clear();
    std::string app_dir = "/opt/tinexus-apps";
    if (std::filesystem::exists(app_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(app_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();
            if (path.ends_with(".sig")) continue;

            int bin_fd = open(path.c_str(), O_RDONLY);
            if (bin_fd >= 0) {
                std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(bin_fd);
                close(bin_fd);

                std::string sig_path = path + ".sig";
                bool verified = false;
                if (std::filesystem::exists(sig_path)) {
                    verified = tinexus::guard::CryptoValidator::verify_signature(
                        path, sig_path, "/etc/tinexus/keys/root.pub");
                }

                if (!verified) {
                    QVariantMap app;
                    app["name"] = QString::fromStdString(entry.path().filename().string());
                    app["path"] = QString::fromStdString(path);
                    app["hash"] = QString::fromStdString(hash);
                    m_unverifiedApps.append(app);
                }
            }
        }
    }
    emit unverifiedAppsChanged();
}

void SettingsBridge::trustApp(const QString& hash) {
    std::string trust_path = "/var/lib/tinexus/trust-overrides.conf";
    std::filesystem::create_directories("/var/lib/tinexus");
    std::ofstream out(trust_path, std::ios::app);
    if (out.is_open()) {
        out << hash.toStdString() << "\n";
        out.close();
    }
    emit toastNotification(QStringLiteral("Granted Capability Token to %1").arg(hash.left(12)), false);
    rescanApps();
}

void SettingsBridge::pollWifiStatus() {
    emit wifiNetworksChanged();
    emit isScanningChanged();
    emit isConnectingChanged();
    emit connectedSsidChanged();
    emit ipAddressChanged();
    emit statusMessageChanged();
    emit connectedSignalBarsChanged();
}

void SettingsBridge::readSystemInfo() {
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
                    m_cpuModel = QString::fromStdString(line.substr(colon + 2));
                    break;
                }
            }
        }
    }
    if (m_cpuModel.isEmpty()) m_cpuModel = QStringLiteral("Generic x86_64 Processor");
    m_compositorInfo = QString::fromStdString(hardware::DisplayUtils::get_compositor_version_string());

    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        double total_bytes = static_cast<double>(vfs.f_blocks) * vfs.f_frsize;
        double free_bytes = static_cast<double>(vfs.f_bavail) * vfs.f_frsize;
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

    emit aboutInfoChanged();
    emit compositorInfoChanged();
}

void SettingsBridge::scanWallpapers() {
    m_wallpapers.clear();
    const struct WallDef {
        const char* name;
        const char* fileName;
        const char* color;
    } walls[] = {
        {"Tinexus Default",    "tinexus-default.jpg",    "#18223c"},
        {"Tinexus OS Primary", "tinexus-os-primary.jpg", "#121b2d"},
        {"Sunset Gradient",    "sunset-gradient.png",    "#4a1d36"},
        {"Emerald Matrix",     "emerald-matrix.png",     "#0f2f2e"}
    };

    for (const auto& w : walls) {
        QVariantMap map;
        map["name"] = QString::fromLatin1(w.name);
        QString path = QStringLiteral("/usr/share/backgrounds/") + QString::fromLatin1(w.fileName);
        if (!QFile::exists(path)) {
            QString alt = QStringLiteral("/home/tinexus/Pictures/") + QString::fromLatin1(w.fileName);
            if (QFile::exists(alt)) {
                path = alt;
            } else {
                path = QDir::current().absoluteFilePath(QStringLiteral("assets/wallpaper/") + QString::fromLatin1(w.fileName));
            }
        }
        map["path"] = path;
        map["previewColor"] = QString::fromLatin1(w.color);
        m_wallpapers.append(map);
    }
    emit wallpapersChanged();
}

void SettingsBridge::loadConfig() {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string config_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
    std::string config_path = config_dir + "/tinexus/settings.toml";

    std::ifstream in(config_path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        if (line.starts_with("accent_index")) {
            try {
                m_accentIndex = std::clamp(std::stoi(line.substr(pos + 1)), 0, ACCENT_COUNT - 1);
            } catch (...) {}
        } else if (line.starts_with("selected_wallpaper_idx")) {
            try {
                m_selectedWallpaperIndex = std::clamp(std::stoi(line.substr(pos + 1)), 0, static_cast<int>(m_wallpapers.size() - 1));
            } catch (...) {}
        } else if (line.starts_with("theme_mode")) {
            std::string mode = line.substr(pos + 1);
            mode.erase(0, mode.find_first_not_of(" \t\""));
            mode.erase(mode.find_last_not_of(" \t\"") + 1);
            if (!mode.empty()) m_themeMode = QString::fromStdString(mode);
        } else if (line.starts_with("display_scale_idx")) {
            try {
                m_displayScaleIndex = std::clamp(std::stoi(line.substr(pos + 1)), 0, 3);
            } catch (...) {}
        } else if (line.starts_with("night_light")) {
            m_nightLight = (line.find("true") != std::string::npos);
        } else if (line.starts_with("vrr_enabled")) {
            m_vrrEnabled = (line.find("true") != std::string::npos);
        } else if (line.starts_with("screen_timeout_min")) {
            try {
                m_screenTimeoutMin = std::clamp(std::stoi(line.substr(pos + 1)), 1, 60);
            } catch (...) {}
        } else if (line.starts_with("sleep_after_min")) {
            try {
                m_sleepAfterMin = std::clamp(std::stoi(line.substr(pos + 1)), 5, 120);
            } catch (...) {}
        } else if (line.starts_with("power_profile_idx")) {
            try {
                m_powerProfileIndex = std::clamp(std::stoi(line.substr(pos + 1)), 0, 2);
            } catch (...) {}
        } else if (line.starts_with("lock_on_sleep")) {
            m_lockOnSleep = (line.find("true") != std::string::npos);
        } else if (line.starts_with("pam_auth")) {
            m_pamAuth = (line.find("true") != std::string::npos);
        } else if (line.starts_with("clipboard_history_size")) {
            try {
                m_clipboardHistorySize = std::stoi(line.substr(pos + 1));
            } catch (...) {}
        }
    }
}

void SettingsBridge::saveConfig() {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string config_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
    std::filesystem::create_directories(config_dir + "/tinexus");
    std::string config_path = config_dir + "/tinexus/settings.toml";

    std::string tmp_path = config_path + ".tmp";
    std::ofstream out(tmp_path);
    if (!out.is_open()) return;

    out << "# Tinexus Desktop Settings Configuration (Qt6/QML)\n";
    out << "accent_index = " << m_accentIndex << "\n";
    out << "selected_wallpaper_idx = " << m_selectedWallpaperIndex << "\n";
    if (m_selectedWallpaperIndex >= 0 && m_selectedWallpaperIndex < m_wallpapers.size()) {
        out << "wallpaper_path = \"" << m_wallpapers[m_selectedWallpaperIndex].toMap().value(QStringLiteral("path")).toString().toStdString() << "\"\n";
    }
    out << "theme_mode = \"" << m_themeMode.toStdString() << "\"\n";
    out << "display_scale_idx = " << m_displayScaleIndex << "\n";
    out << "night_light = " << (m_nightLight ? "true" : "false") << "\n";
    out << "vrr_enabled = " << (m_vrrEnabled ? "true" : "false") << "\n";
    out << "screen_timeout_min = " << m_screenTimeoutMin << "\n";
    out << "sleep_after_min = " << m_sleepAfterMin << "\n";
    out << "power_profile_idx = " << m_powerProfileIndex << "\n";
    out << "lock_on_sleep = " << (m_lockOnSleep ? "true" : "false") << "\n";
    out << "pam_auth = " << (m_pamAuth ? "true" : "false") << "\n";
    out << "clipboard_history_size = " << m_clipboardHistorySize << "\n";
    out.close();

    std::filesystem::rename(tmp_path, config_path);
}

} // namespace tinexus::settings_ui
