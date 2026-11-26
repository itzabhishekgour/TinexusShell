#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace tinexus::settings_ui {

class SystemController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int screenTimeoutMin READ screenTimeoutMin WRITE setScreenTimeoutMin NOTIFY screenTimeoutMinChanged)
    Q_PROPERTY(int sleepAfterMin READ sleepAfterMin WRITE setSleepAfterMin NOTIFY sleepAfterMinChanged)
    Q_PROPERTY(int powerProfileIndex READ powerProfileIndex WRITE setPowerProfileIndex NOTIFY powerProfileIndexChanged)
    Q_PROPERTY(bool lockOnSleep READ lockOnSleep WRITE setLockOnSleep NOTIFY lockOnSleepChanged)
    Q_PROPERTY(bool pamAuth READ pamAuth WRITE setPamAuth NOTIFY pamAuthChanged)
    Q_PROPERTY(int clipboardHistorySize READ clipboardHistorySize WRITE setClipboardHistorySize NOTIFY clipboardHistorySizeChanged)

    Q_PROPERTY(bool hasBattery READ hasBattery NOTIFY batteryStatusChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryStatusChanged)
    Q_PROPERTY(QString batteryState READ batteryState NOTIFY batteryStatusChanged)
    Q_PROPERTY(bool onBattery READ onBattery NOTIFY batteryStatusChanged)

public:
    explicit SystemController(QObject* parent = nullptr);
    ~SystemController() override = default;

    int screenTimeoutMin() const { return m_screenTimeoutMin; }
    int sleepAfterMin() const { return m_sleepAfterMin; }
    int powerProfileIndex() const { return m_powerProfileIndex; }
    bool lockOnSleep() const { return m_lockOnSleep; }
    bool pamAuth() const { return m_pamAuth; }
    int clipboardHistorySize() const { return m_clipboardHistorySize; }

    bool hasBattery() const { return m_hasBattery; }
    int batteryPercent() const { return m_batteryPercent; }
    QString batteryState() const { return m_batteryState; }
    bool onBattery() const { return m_onBattery; }

public slots:
    void setScreenTimeoutMin(int min);
    void setSleepAfterMin(int min);
    void setPowerProfileIndex(int index);
    void setLockOnSleep(bool enabled);
    void setPamAuth(bool enabled);
    void setClipboardHistorySize(int size);
    void clearClipboardHistory();
    void refreshPowerStatus();

    void sessionLock();
    void sessionSuspend();
    void sessionReboot();
    void sessionShutdown();

signals:
    void screenTimeoutMinChanged();
    void sleepAfterMinChanged();
    void powerProfileIndexChanged();
    void lockOnSleepChanged();
    void pamAuthChanged();
    void clipboardHistorySizeChanged();
    void batteryStatusChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    int m_screenTimeoutMin{15};
    int m_sleepAfterMin{30};
    int m_powerProfileIndex{1};
    bool m_lockOnSleep{true};
    bool m_pamAuth{true};
    int m_clipboardHistorySize{50};

    bool m_hasBattery{false};
    int m_batteryPercent{100};
    QString m_batteryState{"AC Power"};
    bool m_onBattery{false};
};

} // namespace tinexus::settings_ui
