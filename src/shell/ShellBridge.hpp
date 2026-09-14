// ============================================================================
// ShellBridge.hpp — C++20 QObject Bridge for tinexus-shell
// Ref: 05_UI_UX_GUIDELINES.md §6.4, 06_COMPONENT_DESIGN.md §8
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

namespace tinexus::shell {

class ShellBridge : public QObject {
    Q_OBJECT

    // Dynamic System Properties
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY timeChanged)
    Q_PROPERTY(QString currentDate READ currentDate NOTIFY timeChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryChanged)
    Q_PROPERTY(bool networkConnected READ networkConnected NOTIFY networkChanged)
    Q_PROPERTY(int networkBars READ networkBars NOTIFY networkChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool soundMuted READ soundMuted WRITE setSoundMuted NOTIFY volumeChanged)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(QString activeAppName READ activeAppName WRITE setActiveAppName NOTIFY activeAppChanged)
    Q_PROPERTY(QString logoUrl READ logoUrl CONSTANT)

    // Flyout & Dialog Visibility States
    Q_PROPERTY(bool logoMenuOpen READ logoMenuOpen WRITE setLogoMenuOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool appMenuOpen READ appMenuOpen WRITE setAppMenuOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool calendarOpen READ calendarOpen WRITE setCalendarOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool notificationsOpen READ notificationsOpen WRITE setNotificationsOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool volumeFlyoutOpen READ volumeFlyoutOpen WRITE setVolumeFlyoutOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool brightnessFlyoutOpen READ brightnessFlyoutOpen WRITE setBrightnessFlyoutOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool rebootConfirmationOpen READ rebootConfirmationOpen WRITE setRebootConfirmationOpen NOTIFY flyoutStateChanged)
    Q_PROPERTY(bool shutdownConfirmationOpen READ shutdownConfirmationOpen WRITE setShutdownConfirmationOpen NOTIFY flyoutStateChanged)

    Q_PROPERTY(bool notchExpanded READ notchExpanded WRITE setNotchExpanded NOTIFY notchExpandedChanged)
    Q_PROPERTY(bool mediaPlaying READ mediaPlaying NOTIFY mediaStateChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaStateChanged)
    Q_PROPERTY(QString mediaArtist READ mediaArtist NOTIFY mediaStateChanged)
    Q_PROPERTY(qreal mediaProgress READ mediaProgress NOTIFY mediaStateChanged)
    Q_PROPERTY(QString mediaTimeStr READ mediaTimeStr NOTIFY mediaStateChanged)

    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(QVariantList applicationsList READ applicationsList NOTIFY applicationsListChanged)

public:
    explicit ShellBridge(QObject* parent = nullptr);
    ~ShellBridge() override = default;

    // Getters
    QString currentTime() const { return m_currentTime; }
    QString currentDate() const { return m_currentDate; }
    int batteryPercent() const { return m_batteryPercent; }
    bool networkConnected() const { return m_networkConnected; }
    int networkBars() const { return m_networkBars; }
    int volume() const { return m_volume; }
    bool soundMuted() const { return m_soundMuted; }
    int brightness() const { return m_brightness; }
    QString activeAppName() const { return m_activeAppName; }
    QString logoUrl() const { return m_logoUrl; }

    bool logoMenuOpen() const { return m_logoMenuOpen; }
    bool appMenuOpen() const { return m_appMenuOpen; }
    bool calendarOpen() const { return m_calendarOpen; }
    bool notificationsOpen() const { return m_notificationsOpen; }
    bool volumeFlyoutOpen() const { return m_volumeFlyoutOpen; }
    bool brightnessFlyoutOpen() const { return m_brightnessFlyoutOpen; }
    bool rebootConfirmationOpen() const { return m_rebootConfirmationOpen; }
    bool shutdownConfirmationOpen() const { return m_shutdownConfirmationOpen; }

    bool notchExpanded() const { return m_notchExpanded; }
    bool mediaPlaying() const { return m_mediaPlaying; }
    QString mediaTitle() const { return m_mediaTitle; }
    QString mediaArtist() const { return m_mediaArtist; }
    qreal mediaProgress() const { return m_mediaProgress; }
    QString mediaTimeStr() const { return m_mediaTimeStr; }

    QVariantList notifications() const { return m_notifications; }
    QVariantList applicationsList() const { return m_applicationsList; }

    // Setters
    void setVolume(int vol);
    void setSoundMuted(bool muted);
    void setBrightness(int bri);
    void setActiveAppName(const QString& name);
    void syncAudioState();

    void setNotchExpanded(bool exp);
    void setLogoMenuOpen(bool open);
    void setAppMenuOpen(bool open);
    void setCalendarOpen(bool open);
    void setNotificationsOpen(bool open);
    void setVolumeFlyoutOpen(bool open);
    void setBrightnessFlyoutOpen(bool open);
    void setRebootConfirmationOpen(bool open);
    void setShutdownConfirmationOpen(bool open);

    // QML Invokables
    Q_INVOKABLE void closeAllFlyouts();
    Q_INVOKABLE void toggleLogoMenu();
    Q_INVOKABLE void toggleAppMenu();
    Q_INVOKABLE void toggleCalendar();
    Q_INVOKABLE void toggleNotifications();
    Q_INVOKABLE void toggleVolume();
    Q_INVOKABLE void toggleBrightness();
    Q_INVOKABLE void clearNotifications();
    Q_INVOKABLE void dismissNotification(int index);
    Q_INVOKABLE void toggleMediaPlayback();
    Q_INVOKABLE void nextMediaTrack();
    Q_INVOKABLE void prevMediaTrack();
    Q_INVOKABLE void launchApp(const QString& execCmd);
    Q_INVOKABLE void openWifiSettings();
    Q_INVOKABLE void onNotchCenterClicked();
    Q_INVOKABLE void requestBlur(bool enable = true, int radius = 28);

    // Power Actions
    Q_INVOKABLE void requestReboot();
    Q_INVOKABLE void requestShutdown();
    Q_INVOKABLE void powerReboot();
    Q_INVOKABLE void powerShutdown();
    Q_INVOKABLE void powerSleep();

signals:
    void timeChanged();
    void batteryChanged();
    void networkChanged();
    void volumeChanged();
    void brightnessChanged();
    void activeAppChanged();
    void flyoutStateChanged();
    void notificationsChanged();
    void applicationsListChanged();
    void notchExpandedChanged();
    void mediaStateChanged();

private slots:
    void updateClock();

private:
    void scanApplications();
    void pollBattery();
    void resolveLogoUrl();

    QString m_currentTime{"12:00 PM"};
    QString m_currentDate{"Wed, Sep 10"};
    int     m_batteryPercent{-1};
    bool    m_networkConnected{true};
    int     m_networkBars{4};
    int     m_volume{75};
    bool    m_soundMuted{false};
    int     m_brightness{80};
    QString m_activeAppName{"Applications"};
    QString m_logoUrl;

    bool    m_notchExpanded{false};
    bool    m_mediaPlaying{true};
    QString m_mediaTitle{"Stargazing"};
    QString m_mediaArtist{"Tinexus Sound System"};
    qreal   m_mediaProgress{0.48};
    QString m_mediaTimeStr{"1:42 / 3:30"};

    bool    m_logoMenuOpen{false};
    bool    m_appMenuOpen{false};
    bool    m_calendarOpen{false};
    bool    m_notificationsOpen{false};
    bool    m_volumeFlyoutOpen{false};
    bool    m_brightnessFlyoutOpen{false};
    bool    m_rebootConfirmationOpen{false};
    bool    m_shutdownConfirmationOpen{false};

    QVariantList m_notifications;
    QVariantList m_applicationsList;
    QTimer       m_clockTimer;
};

} // namespace tinexus::shell
