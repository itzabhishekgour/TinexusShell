#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace tinexus::settings_ui {

class DisplayController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int displayScaleIndex READ displayScaleIndex WRITE setDisplayScaleIndex NOTIFY displayScaleIndexChanged)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(bool nightLight READ nightLight WRITE setNightLight NOTIFY nightLightChanged)
    Q_PROPERTY(bool vrrEnabled READ vrrEnabled WRITE setVrrEnabled NOTIFY vrrEnabledChanged)
    Q_PROPERTY(QString compositorInfo READ compositorInfo NOTIFY compositorInfoChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString refreshRate READ refreshRate NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString connectorName READ connectorName NOTIFY displayHardwareChanged)
    Q_PROPERTY(QString displaySubtitle READ displaySubtitle NOTIFY displayHardwareChanged)

public:
    explicit DisplayController(QObject* parent = nullptr);
    ~DisplayController() override = default;

    int displayScaleIndex() const { return m_displayScaleIndex; }
    int brightness() const { return m_brightness; }
    bool nightLight() const { return m_nightLight; }
    bool vrrEnabled() const { return m_vrrEnabled; }
    QString compositorInfo() const { return m_compositorInfo; }
    QString resolution() const { return m_resolution; }
    QString refreshRate() const { return m_refreshRate; }
    QString connectorName() const { return m_connectorName; }
    QString displaySubtitle() const { return m_displaySubtitle; }

public slots:
    void setDisplayScaleIndex(int index);
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
    void settingModified(const QString& key, const QVariant& value);

private:
    int m_displayScaleIndex{0};
    int m_brightness{100};
    bool m_nightLight{false};
    bool m_vrrEnabled{false};
    QString m_compositorInfo;
    QString m_resolution{"1920 × 1080"};
    QString m_refreshRate{"60 Hz"};
    QString m_connectorName{"Built-in Display"};
    QString m_displaySubtitle{"Built-in Display  •  60 Hz"};
};

} // namespace tinexus::settings_ui
