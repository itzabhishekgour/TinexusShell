#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QDBusVariant>
#include <memory>

#include "controllers/DisplayController.hpp"
#include "controllers/AudioController.hpp"
#include "controllers/NetworkController.hpp"
#include "controllers/PersonalizationController.hpp"
#include "controllers/SystemController.hpp"
#include "controllers/AboutController.hpp"
#include "controllers/PrivacyController.hpp"
#include "controllers/SearchController.hpp"

namespace tinexus::settings_ui {

class SettingsBridge : public QObject {
    Q_OBJECT

    // ── Navigation ────────────────────────────────────────────────────────
    Q_PROPERTY(int currentPage READ currentPage WRITE selectPage NOTIFY currentPageChanged)

    // ── Display ───────────────────────────────────────────────────────────
    Q_PROPERTY(int displayScaleIndex READ displayScaleIndex WRITE setDisplayScaleIndex NOTIFY displayScaleIndexChanged)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(bool nightLight READ nightLight WRITE setNightLight NOTIFY nightLightChanged)
    Q_PROPERTY(bool vrrEnabled READ vrrEnabled WRITE setVrrEnabled NOTIFY vrrEnabledChanged)
    Q_PROPERTY(QString compositorInfo READ compositorInfo NOTIFY compositorInfoChanged)
    Q_PROPERTY(QString displayResolution READ displayResolution NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString displayRefreshRate READ displayRefreshRate NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString displayConnector READ displayConnector NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString displaySubtitle READ displaySubtitle NOTIFY displayHardwareChanged)

    // ── Sound ─────────────────────────────────────────────────────────────
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY outputDevicesChanged)
    Q_PROPERTY(int currentOutputIndex READ currentOutputIndex WRITE setOutputDevice NOTIFY outputDeviceChanged)

    // ── Personalization ───────────────────────────────────────────────────
    Q_PROPERTY(int accentIndex READ accentIndex WRITE setAccentIndex NOTIFY accentIndexChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(int selectedWallpaperIndex READ selectedWallpaperIndex WRITE setSelectedWallpaperIndex NOTIFY selectedWallpaperIndexChanged)
    Q_PROPERTY(int wallpaperIndex READ selectedWallpaperIndex WRITE setSelectedWallpaperIndex NOTIFY selectedWallpaperIndexChanged)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY wallpapersChanged)

    // ── Network & Wi-Fi ───────────────────────────────────────────────────
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY isConnectingChanged)
    Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY connectedSsidChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY connectingSsidChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
    Q_PROPERTY(QString activeInterface READ activeInterface NOTIFY activeInterfaceChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int connectedSignalBars READ connectedSignalBars NOTIFY connectedSignalBarsChanged)
    Q_PROPERTY(QVariantList networkInterfaces READ networkInterfaces NOTIFY networkInterfacesChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiNetworksChanged)

    // ── System & Power ────────────────────────────────────────────────────
    Q_PROPERTY(int screenTimeoutMin READ screenTimeoutMin WRITE setScreenTimeoutMin NOTIFY screenTimeoutMinChanged)
    Q_PROPERTY(int sleepAfterMin READ sleepAfterMin WRITE setSleepAfterMin NOTIFY sleepAfterMinChanged)
    Q_PROPERTY(int powerProfileIndex READ powerProfileIndex WRITE setPowerProfileIndex NOTIFY powerProfileIndexChanged)
    Q_PROPERTY(bool lockOnSleep READ lockOnSleep WRITE setLockOnSleep NOTIFY lockOnSleepChanged)
    Q_PROPERTY(bool pamAuth READ pamAuth WRITE setPamAuth NOTIFY pamAuthChanged)
    Q_PROPERTY(int clipboardHistorySize READ clipboardHistorySize WRITE setClipboardHistorySize NOTIFY clipboardHistorySizeChanged)

    // ── Privacy & Security ────────────────────────────────────────────────
    Q_PROPERTY(QVariantList unverifiedApps READ unverifiedApps NOTIFY unverifiedAppsChanged)
    Q_PROPERTY(int unverifiedAppsCount READ unverifiedAppsCount NOTIFY unverifiedAppsChanged)

    // ── About & System Specs ──────────────────────────────────────────────
    Q_PROPERTY(QString osVersion READ osVersion NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString memInfo READ memInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString storageInfo READ storageInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString displayInfo READ displayInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString graphicsEngine READ graphicsEngine NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString platformVersion READ platformVersion NOTIFY aboutInfoChanged)

    // ── Real User Information ─────────────────────────────────────────────
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY userInfoChanged)
    Q_PROPERTY(QString currentUserRealName READ currentUserRealName NOTIFY userInfoChanged)
    Q_PROPERTY(QString userInitials READ userInitials NOTIFY userInfoChanged)

    // ── Search Engine ─────────────────────────────────────────────────────
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY searchQueryChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)

public:
    explicit SettingsBridge(QObject* parent = nullptr);
    ~SettingsBridge() override;

    // Navigation
    int currentPage() const { return m_currentPage; }

    // Display
    int displayScaleIndex() const { return m_display->displayScaleIndex(); }
    int brightness() const { return m_display->brightness(); }
    bool nightLight() const { return m_display->nightLight(); }
    bool vrrEnabled() const { return m_display->vrrEnabled(); }
    QString compositorInfo() const { return m_display->compositorInfo(); }
    QString displayResolution() const { return m_display->resolution(); }
    QString displayRefreshRate() const { return m_display->refreshRate(); }
    QString displayConnector() const { return m_display->connectorName(); }
    QString displaySubtitle() const { return m_display->displaySubtitle(); }

    // Sound
    int volume() const { return m_audio->volume(); }
    bool muted() const { return m_audio->muted(); }
    QStringList outputDevices() const { return m_audio->outputDevices(); }
    int currentOutputIndex() const { return m_audio->currentOutputIndex(); }

    // Personalization
    int accentIndex() const { return m_personalization->accentIndex(); }
    QString accentColor() const { return m_personalization->accentColor(); }
    QString themeMode() const { return m_personalization->themeMode(); }
    int selectedWallpaperIndex() const { return m_personalization->selectedWallpaperIndex(); }
    QVariantList wallpapers() const { return m_personalization->wallpapers(); }

    // Network
    bool wifiEnabled() const { return m_network->wifiEnabled(); }
    bool isScanning() const { return m_network->isScanning(); }
    bool isConnecting() const { return m_network->isConnecting(); }
    QString connectedSsid() const { return m_network->connectedSsid(); }
    QString connectingSsid() const { return m_network->connectingSsid(); }
    QString ipAddress() const { return m_network->ipAddress(); }
    QString activeInterface() const { return m_network->activeInterface(); }
    QString statusMessage() const { return m_network->statusMessage(); }
    int connectedSignalBars() const { return m_network->connectedSignalBars(); }
    QVariantList networkInterfaces() const { return m_network->networkInterfaces(); }
    QVariantList wifiNetworks() const { return m_network->wifiNetworks(); }

    // System
    int screenTimeoutMin() const { return m_system->screenTimeoutMin(); }
    int sleepAfterMin() const { return m_system->sleepAfterMin(); }
    int powerProfileIndex() const { return m_system->powerProfileIndex(); }
    bool lockOnSleep() const { return m_system->lockOnSleep(); }
    bool pamAuth() const { return m_system->pamAuth(); }
    int clipboardHistorySize() const { return m_system->clipboardHistorySize(); }

    // Privacy
    QVariantList unverifiedApps() const { return m_privacy->unverifiedApps(); }
    int unverifiedAppsCount() const { return m_privacy->unverifiedAppsCount(); }

    // About
    QString osVersion() const { return m_about->osVersion(); }
    QString cpuModel() const { return m_about->cpuModel(); }
    QString memInfo() const { return m_about->memInfo(); }
    QString storageInfo() const { return m_about->storageInfo(); }
    QString displayInfo() const { return m_about->displayInfo(); }
    QString graphicsEngine() const { return m_about->graphicsEngine(); }
    QString platformVersion() const { return m_about->platformVersion(); }

    // User Profile
    QString currentUserName() const { return m_about->currentUserName(); }
    QString currentUserRealName() const { return m_about->currentUserRealName(); }
    QString userInitials() const { return m_about->userInitials(); }

    // Search
    QString searchQuery() const { return m_search->searchQuery(); }
    bool isSearching() const { return m_search->isSearching(); }
    QVariantList searchResults() const { return m_search->searchResults(); }
    Q_INVOKABLE bool isPageMatching(int pageIndex) const { return m_search->isPageMatching(pageIndex); }

public slots:
    // Navigation
    void selectPage(int page);

    // Display
    void setDisplayScaleIndex(int index) { m_display->setDisplayScaleIndex(index); }
    void setBrightness(int percent) { m_display->setBrightness(percent); }
    void setNightLight(bool enabled) { m_display->setNightLight(enabled); }
    void setVrrEnabled(bool enabled) { m_display->setVrrEnabled(enabled); }

    // Sound
    void setVolume(int vol) { m_audio->setVolume(vol); }
    void setMuted(bool muted) { m_audio->setMuted(muted); }
    void setOutputDevice(int index) { m_audio->setOutputDevice(index); }
    void refreshAudioDevices() { m_audio->refreshAudioDevices(); }
    void syncAudioState() { m_audio->syncAudioState(); }
    void playTestSound() { m_audio->playTestSound(); }

    // Personalization
    void setAccentIndex(int index) { m_personalization->setAccentIndex(index); }
    void setThemeMode(const QString& mode) { m_personalization->setThemeMode(mode); }
    void setSelectedWallpaperIndex(int index) { m_personalization->setSelectedWallpaperIndex(index); }

    // Network
    void setWifiEnabled(bool enabled) { m_network->setWifiEnabled(enabled); }
    void triggerWifiScan() { m_network->triggerWifiScan(); }
    void connectWifi(const QString& ssid, const QString& password) { m_network->connectWifi(ssid, password); }
    void disconnectWifi() { m_network->disconnectWifi(); }
    void refreshNetworkInterfaces() { m_network->refreshNetworkInterfaces(); }
    void pollWifiStatus() { m_network->pollWifiStatus(); }

    // System
    void setScreenTimeoutMin(int min) { m_system->setScreenTimeoutMin(min); }
    void setSleepAfterMin(int min) { m_system->setSleepAfterMin(min); }
    void setPowerProfileIndex(int index) { m_system->setPowerProfileIndex(index); }
    void setLockOnSleep(bool enabled) { m_system->setLockOnSleep(enabled); }
    void setPamAuth(bool enabled) { m_system->setPamAuth(enabled); }
    void setClipboardHistorySize(int size) { m_system->setClipboardHistorySize(size); }
    void clearClipboardHistory() { m_system->clearClipboardHistory(); }
    void sessionLock() { m_system->sessionLock(); }
    void sessionSuspend() { m_system->sessionSuspend(); }
    void sessionReboot() { m_system->sessionReboot(); }
    void sessionShutdown() { m_system->sessionShutdown(); }

    // Privacy
    void rescanApps() { m_privacy->rescanApps(); }
    void refreshUnverifiedApps() { m_privacy->rescanApps(); }
    void trustApp(const QString& hash) { m_privacy->trustApp(hash); }

    // About
    void checkForUpdates() { m_about->checkForUpdates(); }

    // Search
    void setSearchQuery(const QString& query) { m_search->setSearchQuery(query); }
    void clearSearch() { m_search->clearSearch(); }

    // Wi-Fi Modal triggers
    void openWifiModal(const QString& ssid) { emit requestWifiModal(ssid); }
    void closeWifiModal() { emit dismissWifiModal(); }

    // Persistence & IPC sync
    void loadConfig();
    void saveConfig();

    // D-Bus Signal Handler
    void onDaemonConfigChanged(const QString& key, const QDBusVariant& value);

signals:
    void requestWifiModal(const QString& ssid);
    void dismissWifiModal();

    void currentPageChanged();
    void displayScaleIndexChanged();
    void brightnessChanged();
    void nightLightChanged();
    void vrrEnabledChanged();
    void compositorInfoChanged();
    void displayHardwareChanged();

    void volumeChanged();
    void mutedChanged();
    void outputDevicesChanged();
    void outputDeviceChanged();

    void accentIndexChanged();
    void accentColorChanged();
    void themeModeChanged();
    void selectedWallpaperIndexChanged();
    void wallpapersChanged();

    void wifiEnabledChanged();
    void isScanningChanged();
    void isConnectingChanged();
    void connectedSsidChanged();
    void connectingSsidChanged();
    void ipAddressChanged();
    void activeInterfaceChanged();
    void statusMessageChanged();
    void connectedSignalBarsChanged();
    void networkInterfacesChanged();
    void wifiNetworksChanged();

    void screenTimeoutMinChanged();
    void sleepAfterMinChanged();
    void powerProfileIndexChanged();
    void lockOnSleepChanged();
    void pamAuthChanged();
    void clipboardHistorySizeChanged();

    void unverifiedAppsChanged();
    void aboutInfoChanged();
    void userInfoChanged();
    void searchQueryChanged();
    void searchResultsChanged();

    void toastNotification(const QString& message, bool isError);

private:
    void initControllers();
    void initDbusClient();
    void syncSettingToDaemon(const QString& key, const QVariant& value);

    int m_currentPage{0};

    std::unique_ptr<DisplayController> m_display;
    std::unique_ptr<AudioController> m_audio;
    std::unique_ptr<NetworkController> m_network;
    std::unique_ptr<PersonalizationController> m_personalization;
    std::unique_ptr<SystemController> m_system;
    std::unique_ptr<AboutController> m_about;
    std::unique_ptr<PrivacyController> m_privacy;
    std::unique_ptr<SearchController> m_search;
};

} // namespace tinexus::settings_ui
