#include "AudioController.hpp"
#include <common/AudioUtils.hpp>
#include <common/logger.hpp>
#include <algorithm>

namespace tinexus::settings_ui {

AudioController::AudioController(QObject* parent)
    : QObject(parent)
{
    m_volume = hardware::AudioUtils::get_volume_percent();
    m_muted = hardware::AudioUtils::is_muted();
    refreshAudioDevices();

    connect(&m_syncTimer, &QTimer::timeout, this, &AudioController::syncAudioState);
    m_syncTimer.start(300);
}

void AudioController::setVolume(int vol) {
    vol = std::clamp(vol, 0, 100);
    if (m_volume != vol) {
        m_volume = vol;
        hardware::AudioUtils::set_volume_percent(vol, /*persist=*/true, /*throttle=*/true);
        emit volumeChanged();
        emit settingModified(QStringLiteral("volume"), vol);
    }
}

void AudioController::setMuted(bool isMuted) {
    if (m_muted != isMuted) {
        m_muted = isMuted;
        if (hardware::AudioUtils::is_muted() != m_muted) {
            hardware::AudioUtils::toggle_mute(/*persist=*/true);
        }
        emit mutedChanged();
        emit toastRequested(m_muted ? QStringLiteral("Audio output muted") : QStringLiteral("Audio output unmuted"), false);
        emit settingModified(QStringLiteral("muted"), isMuted);
    }
}

void AudioController::setOutputDevice(int index) {
    if (index >= 0 && index < m_outputDevices.size()) {
        hardware::AudioUtils::set_output_device_by_index(index);
        m_currentOutputIndex = index;
        emit outputDeviceChanged();
        m_volume = hardware::AudioUtils::get_volume_percent();
        m_muted = hardware::AudioUtils::is_muted();
        emit volumeChanged();
        emit mutedChanged();
        emit toastRequested(QStringLiteral("Audio routed to: ") + m_outputDevices.at(index), false);
        emit settingModified(QStringLiteral("output_device_index"), index);
    }
}

void AudioController::refreshAudioDevices() {
    auto devs = hardware::AudioUtils::get_output_devices();
    m_outputDevices.clear();
    for (const auto& d : devs) {
        m_outputDevices.append(QString::fromStdString(d));
    }
    if (m_outputDevices.isEmpty()) {
        m_outputDevices.append(QStringLiteral("Default Audio Output"));
    }
    m_currentOutputIndex = std::clamp(hardware::AudioUtils::get_current_output_device_index(), 0, static_cast<int>(m_outputDevices.size() - 1));
    emit outputDevicesChanged();
    emit outputDeviceChanged();
}

void AudioController::syncAudioState() {
    int cur_vol = hardware::AudioUtils::get_volume_percent();
    bool cur_muted = hardware::AudioUtils::is_muted();
    if (m_volume != cur_vol) {
        m_volume = cur_vol;
        emit volumeChanged();
    }
    if (m_muted != cur_muted) {
        m_muted = cur_muted;
        emit mutedChanged();
    }
}

void AudioController::playTestSound() {
    hardware::AudioUtils::play_chime();
    emit toastRequested(QStringLiteral("Playing system test sound..."), false);
}

} // namespace tinexus::settings_ui
