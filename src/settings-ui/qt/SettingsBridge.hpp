#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

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

    // ── Sound ─────────────────────────────────────────────────────────────
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

    // ── Personalization ───────────────────────────────────────────────────
    Q_PROPERTY(int accentIndex READ accentIndex WRITE setAccentIndex NOTIFY accentIndexChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(int selectedWallpaperIndex READ selectedWallpaperIndex WRITE setSelectedWallpaperIndex NOTIFY selectedWallpaperIndexChanged)
    Q_PROPERTY(int wallpaperIndex READ selectedWallpaperIndex WRITE setSelectedWallpaperIndex NOTIFY selectedWallpaperIndexChanged)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY wallpapersChanged)

    Q_INVOKABLE void setWallpaperIndex(int index) { setSelectedWallpaperIndex(index); }

    // ── Network & Wi-Fi ───────────────────────────────────────────────────
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY isConnectingChanged)
    Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY connectedSsidChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY connectingSsidChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
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

    // ── About ─────────────────────────────────────────────────────────────
    Q_PROPERTY(QString osVersion READ osVersion NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString memInfo READ memInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString storageInfo READ storageInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString displayInfo READ displayInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString graphicsEngine READ graphicsEngine NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString platformVersion READ platformVersion NOTIFY aboutInfoChanged)

public:
    explicit SettingsBridge(QObject* parent = nullptr);
    ~SettingsBridge() override;

    // Getters
    int currentPage() const { return m_currentPage; }
    int displayScaleIndex() const { return m_displayScaleIndex; }
    int brightness() const { return m_brightness; }
    bool nightLight() const { return m_nightLight; }
    bool vrrEnabled() const { return m_vrrEnabled; }
    QString compositorInfo() const { return m_compositorInfo; }

    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }

    int accentIndex() const { return m_accentIndex; }
    QString accentColor() const;
    QString themeMode() const { return m_themeMode; }
    int selectedWallpaperIndex() const { return m_selectedWallpaperIndex; }
    QVariantList wallpapers() const { return m_wallpapers; }

    bool wifiEnabled() const;
    bool isScanning() const;
    bool isConnecting() const;
    QString connectedSsid() const;
    QString connectingSsid() const;
    QString ipAddress() const;
    QString statusMessage() const;
    int connectedSignalBars() const;
    QVariantList networkInterfaces() const { return m_networkInterfaces; }
    QVariantList wifiNetworks() const;

    int screenTimeoutMin() const { return m_screenTimeoutMin; }
    int sleepAfterMin() const { return m_sleepAfterMin; }
    int powerProfileIndex() const { return m_powerProfileIndex; }
    bool lockOnSleep() const { return m_lockOnSleep; }
    bool pamAuth() const { return m_pamAuth; }
    int clipboardHistorySize() const { return m_clipboardHistorySize; }

    QVariantList unverifiedApps() const { return m_unverifiedApps; }

    QString osVersion() const { return m_osVersion; }
    QString cpuModel() const { return m_cpuModel; }
    QString memInfo() const { return m_memInfo; }
    QString storageInfo() const { return m_storageInfo; }
    QString displayInfo() const { return m_displayInfo; }
    QString graphicsEngine() const { return m_graphicsEngine; }
    QString platformVersion() const { return m_platformVersion; }

public slots:
    void selectPage(int page);
    void setDisplayScaleIndex(int index);
    void setBrightness(int percent);
    void setNightLight(bool enabled);
    void setVrrEnabled(bool enabled);

    void setVolume(int percent);
    void setMuted(bool muted);
    void playTestSound();

    void setAccentIndex(int index);
    void setThemeMode(const QString& mode);
    void setSelectedWallpaperIndex(int index);

    void setWifiEnabled(bool enabled);
    void triggerWifiScan();
    void connectWifi(const QString& ssid, const QString& password);
    void disconnectWifi();
    void refreshNetworkInterfaces();

    void setScreenTimeoutMin(int min);
    void setSleepAfterMin(int min);
    void setPowerProfileIndex(int index);
    void setLockOnSleep(bool enabled);
    void setPamAuth(bool enabled);
    void setClipboardHistorySize(int size);

    void sessionLock();
    void sessionSuspend();
    void sessionReboot();
    void sessionShutdown();

    void rescanApps();
    void trustApp(const QString& hash);

    void openWifiModal(const QString& ssid) { emit requestWifiModal(ssid); }
    void closeWifiModal() { emit dismissWifiModal(); }

    void loadConfig();
    void saveConfig();

    // Polling / periodic check for async Wi-Fi updates
    void pollWifiStatus();

signals:
    void requestWifiModal(const QString& ssid);
    void dismissWifiModal();

    void currentPageChanged();
    void displayScaleIndexChanged();
    void brightnessChanged();
    void nightLightChanged();
    void vrrEnabledChanged();
    void compositorInfoChanged();

    void volumeChanged();
    void mutedChanged();

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

    void toastNotification(const QString& message, bool isError);

private:
    void readSystemInfo();
    void scanWallpapers();

    int m_currentPage{0};
    int m_displayScaleIndex{0};
    int m_brightness{100};
    bool m_nightLight{false};
    bool m_vrrEnabled{false};
    QString m_compositorInfo;

    int m_volume{75};
    bool m_muted{false};

    int m_accentIndex{0};
    QString m_themeMode{"Dark"};
    int m_selectedWallpaperIndex{0};
    QVariantList m_wallpapers;

    QVariantList m_networkInterfaces;

    int m_screenTimeoutMin{5};
    int m_sleepAfterMin{15};
    int m_powerProfileIndex{1};
    bool m_lockOnSleep{true};
    bool m_pamAuth{true};
    int m_clipboardHistorySize{50};

    QVariantList m_unverifiedApps;

    QString m_osVersion;
    QString m_cpuModel;
    QString m_memInfo;
    QString m_storageInfo;
    QString m_displayInfo;
    QString m_graphicsEngine;
    QString m_platformVersion;
};

} // namespace tinexus::settings_ui
