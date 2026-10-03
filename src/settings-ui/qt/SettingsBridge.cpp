#include "SettingsBridge.hpp"
#include <common/logger.hpp>
#include <common/DBusNames.hpp>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace tinexus::settings_ui {

SettingsBridge::SettingsBridge(QObject* parent)
    : QObject(parent)
{
    initControllers();
    loadConfig();
    initDbusClient();
}

SettingsBridge::~SettingsBridge() = default;

void SettingsBridge::initControllers() {
    m_display = std::make_unique<DisplayController>(this);
    m_audio = std::make_unique<AudioController>(this);
    m_network = std::make_unique<NetworkController>(this);
    m_personalization = std::make_unique<PersonalizationController>(this);
    m_system = std::make_unique<SystemController>(this);
    m_about = std::make_unique<AboutController>(this);
    m_privacy = std::make_unique<PrivacyController>(this);
    m_search = std::make_unique<SearchController>(this);

    // Forward Display signals
    connect(m_display.get(), &DisplayController::displayScaleIndexChanged, this, &SettingsBridge::displayScaleIndexChanged);
    connect(m_display.get(), &DisplayController::brightnessChanged, this, &SettingsBridge::brightnessChanged);
    connect(m_display.get(), &DisplayController::nightLightChanged, this, &SettingsBridge::nightLightChanged);
    connect(m_display.get(), &DisplayController::vrrEnabledChanged, this, &SettingsBridge::vrrEnabledChanged);
    connect(m_display.get(), &DisplayController::compositorInfoChanged, this, &SettingsBridge::compositorInfoChanged);
    connect(m_display.get(), &DisplayController::displayHardwareChanged, this, &SettingsBridge::displayHardwareChanged);
    connect(m_display.get(), &DisplayController::settingModified, this, &SettingsBridge::syncSettingToDaemon);

    // Forward Audio signals
    connect(m_audio.get(), &AudioController::volumeChanged, this, &SettingsBridge::volumeChanged);
    connect(m_audio.get(), &AudioController::mutedChanged, this, &SettingsBridge::mutedChanged);
    connect(m_audio.get(), &AudioController::outputDevicesChanged, this, &SettingsBridge::outputDevicesChanged);
    connect(m_audio.get(), &AudioController::outputDeviceChanged, this, &SettingsBridge::outputDeviceChanged);
    connect(m_audio.get(), &AudioController::toastRequested, this, &SettingsBridge::toastNotification);
    connect(m_audio.get(), &AudioController::settingModified, this, &SettingsBridge::syncSettingToDaemon);

    // Forward Personalization signals
    connect(m_personalization.get(), &PersonalizationController::accentIndexChanged, this, &SettingsBridge::accentIndexChanged);
    connect(m_personalization.get(), &PersonalizationController::accentColorChanged, this, &SettingsBridge::accentColorChanged);
    connect(m_personalization.get(), &PersonalizationController::themeModeChanged, this, &SettingsBridge::themeModeChanged);
    connect(m_personalization.get(), &PersonalizationController::selectedWallpaperIndexChanged, this, &SettingsBridge::selectedWallpaperIndexChanged);
    connect(m_personalization.get(), &PersonalizationController::wallpapersChanged, this, &SettingsBridge::wallpapersChanged);
    connect(m_personalization.get(), &PersonalizationController::toastRequested, this, &SettingsBridge::toastNotification);
    connect(m_personalization.get(), &PersonalizationController::settingModified, this, &SettingsBridge::syncSettingToDaemon);

    // Forward Network signals
    connect(m_network.get(), &NetworkController::wifiEnabledChanged, this, &SettingsBridge::wifiEnabledChanged);
    connect(m_network.get(), &NetworkController::isScanningChanged, this, &SettingsBridge::isScanningChanged);
    connect(m_network.get(), &NetworkController::isConnectingChanged, this, &SettingsBridge::isConnectingChanged);
    connect(m_network.get(), &NetworkController::connectedSsidChanged, this, &SettingsBridge::connectedSsidChanged);
    connect(m_network.get(), &NetworkController::connectingSsidChanged, this, &SettingsBridge::connectingSsidChanged);
    connect(m_network.get(), &NetworkController::ipAddressChanged, this, &SettingsBridge::ipAddressChanged);
    connect(m_network.get(), &NetworkController::activeInterfaceChanged, this, &SettingsBridge::activeInterfaceChanged);
    connect(m_network.get(), &NetworkController::statusMessageChanged, this, &SettingsBridge::statusMessageChanged);
    connect(m_network.get(), &NetworkController::connectedSignalBarsChanged, this, &SettingsBridge::connectedSignalBarsChanged);
    connect(m_network.get(), &NetworkController::networkInterfacesChanged, this, &SettingsBridge::networkInterfacesChanged);
    connect(m_network.get(), &NetworkController::wifiNetworksChanged, this, &SettingsBridge::wifiNetworksChanged);
    connect(m_network.get(), &NetworkController::toastRequested, this, &SettingsBridge::toastNotification);
    connect(m_network.get(), &NetworkController::settingModified, this, &SettingsBridge::syncSettingToDaemon);

    // Forward System signals
    connect(m_system.get(), &SystemController::screenTimeoutMinChanged, this, &SettingsBridge::screenTimeoutMinChanged);
    connect(m_system.get(), &SystemController::sleepAfterMinChanged, this, &SettingsBridge::sleepAfterMinChanged);
    connect(m_system.get(), &SystemController::powerProfileIndexChanged, this, &SettingsBridge::powerProfileIndexChanged);
    connect(m_system.get(), &SystemController::lockOnSleepChanged, this, &SettingsBridge::lockOnSleepChanged);
    connect(m_system.get(), &SystemController::pamAuthChanged, this, &SettingsBridge::pamAuthChanged);
    connect(m_system.get(), &SystemController::clipboardHistorySizeChanged, this, &SettingsBridge::clipboardHistorySizeChanged);
    connect(m_system.get(), &SystemController::toastRequested, this, &SettingsBridge::toastNotification);
    connect(m_system.get(), &SystemController::settingModified, this, &SettingsBridge::syncSettingToDaemon);

    // Forward Privacy signals
    connect(m_privacy.get(), &PrivacyController::unverifiedAppsChanged, this, &SettingsBridge::unverifiedAppsChanged);
    connect(m_privacy.get(), &PrivacyController::toastRequested, this, &SettingsBridge::toastNotification);

    // Forward About & User signals
    connect(m_about.get(), &AboutController::aboutInfoChanged, this, &SettingsBridge::aboutInfoChanged);
    connect(m_about.get(), &AboutController::userInfoChanged, this, &SettingsBridge::userInfoChanged);
    connect(m_about.get(), &AboutController::toastRequested, this, &SettingsBridge::toastNotification);

    // Forward Search signals
    connect(m_search.get(), &SearchController::searchQueryChanged, this, &SettingsBridge::searchQueryChanged);
    connect(m_search.get(), &SearchController::searchResultsChanged, this, &SettingsBridge::searchResultsChanged);
}

void SettingsBridge::initDbusClient() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.connect(
            tinexus::common::dbus::qservice::Settings(),
            tinexus::common::dbus::qpath::Settings(),
            tinexus::common::dbus::qinterface::Settings(),
            QStringLiteral("ConfigChanged"),
            this,
            SLOT(onDaemonConfigChanged(QString,QDBusVariant))
        );
        // Connect to legacy endpoints as well
        bus.connect(
            tinexus::common::dbus::qservice::legacy::Settings(),
            tinexus::common::dbus::qpath::legacy::Settings(),
            tinexus::common::dbus::qinterface::legacy::Settings(),
            QStringLiteral("ConfigChanged"),
            this,
            SLOT(onDaemonConfigChanged(QString,QDBusVariant))
        );
        bus.connect(
            tinexus::common::dbus::qservice::legacy::Settings(),
            QStringLiteral("/Settings"),
            tinexus::common::dbus::qinterface::legacy::Settings(),
            QStringLiteral("ConfigChanged"),
            this,
            SLOT(onDaemonConfigChanged(QString,QDBusVariant))
        );
        tinexus::log::info("[SettingsBridge] Connected as D-Bus client to {}",
                           tinexus::common::dbus::service::Settings);
    } else {
        tinexus::log::warn("[SettingsBridge] Session D-Bus not available — operating in local standalone mode");
    }
}

void SettingsBridge::selectPage(int page) {
    page = std::clamp(page, 0, 7);
    if (m_currentPage != page) {
        m_currentPage = page;
        emit currentPageChanged();
    }
}

void SettingsBridge::syncSettingToDaemon(const QString& key, const QVariant& value) {
    saveConfig();

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            tinexus::common::dbus::qservice::Settings(),
            tinexus::common::dbus::qpath::Settings(),
            tinexus::common::dbus::qinterface::Settings(),
            QStringLiteral("SetValue")
        );
        msg << key << QVariant::fromValue(QDBusVariant(value));
        bus.send(msg);
    }
}

void SettingsBridge::onDaemonConfigChanged(const QString& key, const QDBusVariant& value) {
    QVariant val = value.variant();
    QString lower = key.toLower();
    if (lower == QStringLiteral("theme") || lower == QStringLiteral("currenttheme")) {
        m_personalization->setThemeMode(val.toString());
    } else if (lower == QStringLiteral("accent_index")) {
        m_personalization->setAccentIndex(val.toInt());
    } else if (lower == QStringLiteral("display_scale_idx")) {
        m_display->setDisplayScaleIndex(val.toInt());
    }
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
                m_personalization->setAccentIndex(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("selected_wallpaper_idx")) {
            try {
                m_personalization->setSelectedWallpaperIndex(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("theme_mode")) {
            std::string mode = line.substr(pos + 1);
            mode.erase(0, mode.find_first_not_of(" \t\""));
            mode.erase(mode.find_last_not_of(" \t\"") + 1);
            if (!mode.empty()) m_personalization->setThemeMode(QString::fromStdString(mode));
        } else if (line.starts_with("display_scale_idx")) {
            try {
                m_display->setDisplayScaleIndex(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("night_light")) {
            m_display->setNightLight(line.find("true") != std::string::npos);
        } else if (line.starts_with("vrr_enabled")) {
            m_display->setVrrEnabled(line.find("true") != std::string::npos);
        } else if (line.starts_with("screen_timeout_min")) {
            try {
                m_system->setScreenTimeoutMin(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("sleep_after_min")) {
            try {
                m_system->setSleepAfterMin(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("power_profile_idx")) {
            try {
                m_system->setPowerProfileIndex(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        } else if (line.starts_with("lock_on_sleep")) {
            m_system->setLockOnSleep(line.find("true") != std::string::npos);
        } else if (line.starts_with("pam_auth")) {
            m_system->setPamAuth(line.find("true") != std::string::npos);
        } else if (line.starts_with("clipboard_history_size")) {
            try {
                m_system->setClipboardHistorySize(std::stoi(line.substr(pos + 1)));
            } catch (...) {}
        }
    }
}

void SettingsBridge::saveConfig() {
    try {
        const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
        std::string config_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
        std::error_code ec;
        std::filesystem::create_directories(config_dir + "/tinexus", ec);
        std::string config_path = config_dir + "/tinexus/settings.toml";

        std::string tmp_path = config_path + ".tmp";
        std::ofstream out(tmp_path);
        if (!out.is_open()) return;

        out << "# Tinexus Desktop Settings Configuration (Qt6/QML)\n";
        out << "accent_index = " << m_personalization->accentIndex() << "\n";
        out << "selected_wallpaper_idx = " << m_personalization->selectedWallpaperIndex() << "\n";
        auto walls = m_personalization->wallpapers();
        int wallIdx = m_personalization->selectedWallpaperIndex();
        if (wallIdx >= 0 && wallIdx < walls.size()) {
            std::string wall_path = walls[wallIdx].toMap().value(QStringLiteral("path")).toString().toStdString();
            out << "wallpaper_path = \"" << wall_path << "\"\n";
            out << "\n[wallpaper]\n";
            out << "path = \"" << wall_path << "\"\n";
            out << "mode = \"fill\"\n";
        }
        out << "theme_mode = \"" << m_personalization->themeMode().toStdString() << "\"\n";
        out << "display_scale_idx = " << m_display->displayScaleIndex() << "\n";
        out << "night_light = " << (m_display->nightLight() ? "true" : "false") << "\n";
        out << "vrr_enabled = " << (m_display->vrrEnabled() ? "true" : "false") << "\n";
        out << "screen_timeout_min = " << m_system->screenTimeoutMin() << "\n";
        out << "sleep_after_min = " << m_system->sleepAfterMin() << "\n";
        out << "power_profile_idx = " << m_system->powerProfileIndex() << "\n";
        out << "lock_on_sleep = " << (m_system->lockOnSleep() ? "true" : "false") << "\n";
        out << "pam_auth = " << (m_system->pamAuth() ? "true" : "false") << "\n";
        out << "clipboard_history_size = " << m_system->clipboardHistorySize() << "\n";
        out.close();

        std::filesystem::rename(tmp_path, config_path, ec);
    } catch (...) {}
}

} // namespace tinexus::settings_ui
