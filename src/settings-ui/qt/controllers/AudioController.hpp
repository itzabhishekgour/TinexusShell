#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <atomic>
#include <memory>

namespace tinexus::settings_ui {

class AudioController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY outputDevicesChanged)
    Q_PROPERTY(int currentOutputIndex READ currentOutputIndex WRITE setOutputDevice NOTIFY outputDeviceChanged)

    Q_PROPERTY(int inputVolume READ inputVolume WRITE setInputVolume NOTIFY inputVolumeChanged)
    Q_PROPERTY(bool inputMuted READ inputMuted WRITE setInputMuted NOTIFY inputMutedChanged)
    Q_PROPERTY(QStringList inputDevices READ inputDevices NOTIFY inputDevicesChanged)
    Q_PROPERTY(int currentInputIndex READ currentInputIndex WRITE setInputDevice NOTIFY inputDeviceChanged)

public:
    explicit AudioController(QObject* parent = nullptr);
    ~AudioController() override = default;

    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QStringList outputDevices() const { return m_outputDevices; }
    int currentOutputIndex() const { return m_currentOutputIndex; }

    int inputVolume() const { return m_inputVolume; }
    bool inputMuted() const { return m_inputMuted; }
    QStringList inputDevices() const { return m_inputDevices; }
    int currentInputIndex() const { return m_currentInputIndex; }

public slots:
    void setVolume(int vol);
    void setMuted(bool isMuted);
    void setOutputDevice(int index);
    void setInputVolume(int vol);
    void setInputMuted(bool isMuted);
    void setInputDevice(int index);
    void playTestSound();
    void refreshAudioDevices();
    void syncAudioState();

signals:
    void volumeChanged();
    void mutedChanged();
    void outputDevicesChanged();
    void outputDeviceChanged();
    void inputVolumeChanged();
    void inputMutedChanged();
    void inputDevicesChanged();
    void inputDeviceChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    int m_volume{75};
    bool m_muted{false};
    QStringList m_outputDevices;
    int m_currentOutputIndex{0};

    int m_inputVolume{75};
    bool m_inputMuted{false};
    QStringList m_inputDevices;
    int m_currentInputIndex{0};

    QTimer m_syncTimer;
    QTimer m_volumeApplyTimer;
    int m_pendingVolume{-1};
    std::shared_ptr<std::atomic<bool>> m_audioBusy{std::make_shared<std::atomic<bool>>(false)};
};

} // namespace tinexus::settings_ui
