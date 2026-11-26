#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <vector>

namespace tinexus::settings_ui {

class DisplayController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int displayScaleIndex READ displayScaleIndex WRITE setDisplayScaleIndex NOTIFY displayScaleIndexChanged)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(bool nightLight READ nightLight WRITE setNightLight NOTIFY nightLightChanged)
    Q_PROPERTY(bool vrrEnabled READ vrrEnabled WRITE setVrrEnabled NOTIFY vrrEnabledChanged)
    Q_PROPERTY(bool vrrSupported READ vrrSupported NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString compositorInfo READ compositorInfo NOTIFY compositorInfoChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString refreshRate READ refreshRate NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString connectorName READ connectorName NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString displaySubtitle READ displaySubtitle NOTIFY displayHardwareChanged)
    Q_PROPERTY(QStringList availableResolutions READ availableResolutions NOTIFY displayHardwareChanged)
    Q_PROPERTY(int currentResolutionIndex READ currentResolutionIndex WRITE setResolutionIndex NOTIFY displayHardwareChanged)
    Q_PROPERTY(QStringList availableRefreshRates READ availableRefreshRates NOTIFY displayHardwareChanged)
    Q_PROPERTY(int currentRefreshIndex READ currentRefreshIndex WRITE setRefreshIndex NOTIFY displayHardwareChanged)

public:
    explicit DisplayController(QObject* parent = nullptr);
    ~DisplayController() override = default;

    int displayScaleIndex() const { return m_displayScaleIndex; }
    int brightness() const { return m_brightness; }
    bool nightLight() const { return m_nightLight; }
    bool vrrEnabled() const { return m_vrrEnabled; }
    bool vrrSupported() const { return m_vrrSupported; }
    QString compositorInfo() const { return m_compositorInfo; }
    QString resolution() const { return m_resolution; }
    QString refreshRate() const { return m_refreshRate; }
    QString connectorName() const { return m_connectorName; }
    QString displaySubtitle() const { return m_displaySubtitle; }
    QStringList availableResolutions() const { return m_availableResolutions; }
    int currentResolutionIndex() const { return m_currentResolutionIndex; }
    QStringList availableRefreshRates() const { return m_availableRefreshRates; }
    int currentRefreshIndex() const { return m_currentRefreshIndex; }

public slots:
    void setDisplayScaleIndex(int index);
    void setResolutionIndex(int index);
    void setRefreshIndex(int index);
    void setBrightness(int percent);
    void setNightLight(bool enabled);
    void setVrrEnabled(bool enabled);
    void refreshHardware();

signals:
    void displayScaleIndexChanged();
    void brightnessChanged();
    void nightLightChanged();
    void vrrEnabledChanged();
    void compositorInfoChanged();
    void displayHardwareChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    struct ModeInfo {
        int width{1920};
        int height{1080};
        int refresh_mhz{60000};
    };

    void sendDisplayIpc(uint8_t flags);
    void parseOutputsTelemetry();
    void rebuildModeLists(int activeW, int activeH, int activeMhz);

    int m_displayScaleIndex{0};
    int m_brightness{100};
    bool m_nightLight{false};
    bool m_vrrEnabled{false};
    bool m_vrrSupported{false};
    QString m_compositorInfo;
    QString m_resolution{"1920 × 1080"};
    QString m_refreshRate{"60 Hz"};
    QString m_connectorName{"Primary Display"};
    QString m_displaySubtitle{"Primary Display  •  60 Hz"};

    std::vector<ModeInfo> m_modes;
    QStringList m_availableResolutions;
    int m_currentResolutionIndex{0};
    QStringList m_availableRefreshRates;
    int m_currentRefreshIndex{0};
    int m_curWidth{1920};
    int m_curHeight{1080};
    int m_curRefreshMhz{60000};
};

} // namespace tinexus::settings_ui
