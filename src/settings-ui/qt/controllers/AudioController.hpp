#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

namespace tinexus::settings_ui {

class AudioController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY outputDevicesChanged)
    Q_PROPERTY(int currentOutputIndex READ currentOutputIndex WRITE setOutputDevice NOTIFY outputDeviceChanged)

public:
    explicit AudioController(QObject* parent = nullptr);
    ~AudioController() override = default;

    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QStringList outputDevices() const { return m_outputDevices; }
    int currentOutputIndex() const { return m_currentOutputIndex; }

public slots:
    void setVolume(int vol);
    void setMuted(bool isMuted);
    void setOutputDevice(int index);
    void playTestSound();
    void refreshAudioDevices();
    void syncAudioState();

signals:
    void volumeChanged();
    void mutedChanged();
    void outputDevicesChanged();
    void outputDeviceChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    int m_volume{75};
    bool m_muted{false};
    QStringList m_outputDevices;
    int m_currentOutputIndex{0};
    QTimer m_syncTimer;
};

} // namespace tinexus::settings_ui
