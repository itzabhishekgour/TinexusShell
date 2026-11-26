#include "AudioController.hpp"
#include <common/AudioUtils.hpp>
#include <common/logger.hpp>
#include <QMetaObject>
#include <QPointer>
#include <algorithm>
#include <thread>

namespace tinexus::settings_ui {

AudioController::AudioController(QObject* parent)
    : QObject(parent)
{
    m_outputDevices.append(QStringLiteral("Detecting Audio Devices..."));
    m_inputDevices.append(QStringLiteral("Detecting Input Devices..."));

    QTimer::singleShot(0, this, [this]() {
        refreshAudioDevices();
        syncAudioState();
    });

    m_volumeApplyTimer.setSingleShot(true);
    m_volumeApplyTimer.setInterval(45);
    connect(&m_volumeApplyTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingVolume < 0 || m_audioBusy->exchange(true)) {
            if (m_pendingVolume >= 0 && !m_volumeApplyTimer.isActive()) {
                m_volumeApplyTimer.start(45);
            }
            return;
        }
        const int target = m_pendingVolume;
        m_pendingVolume = -1;
        QPointer<AudioController> self(this);
        auto busy = m_audioBusy;
        std::thread([self, busy, target]() {
            hardware::AudioUtils::set_volume_percent(target, /*persist=*/true, /*throttle=*/false);
            busy->store(false);
            if (!self) return;
            QMetaObject::invokeMethod(self, [self]() {
                if (self && self->m_pendingVolume >= 0) {
                    self->m_volumeApplyTimer.start(0);
                }
            }, Qt::QueuedConnection);
        }).detach();
    });

    connect(&m_syncTimer, &QTimer::timeout, this, &AudioController::syncAudioState);
    m_syncTimer.start(3000);
}

void AudioController::setVolume(int vol) {
    vol = std::clamp(vol, 0, 100);
    const bool newMuted = (vol == 0);
    if (m_volume != vol || m_muted != newMuted) {
        m_volume = vol;
        m_muted = newMuted;
        emit volumeChanged();
        emit mutedChanged();
        emit settingModified(QStringLiteral("volume"), vol);
    }
    m_pendingVolume = vol;
    if (!m_volumeApplyTimer.isActive()) {
        m_volumeApplyTimer.start(45);
    }
}

void AudioController::setMuted(bool isMuted) {
    if (m_muted == isMuted) return;
    m_muted = isMuted;
    emit mutedChanged();
    emit toastRequested(m_muted ? QStringLiteral("Audio output muted") : QStringLiteral("Audio output unmuted"), false);
    emit settingModified(QStringLiteral("muted"), isMuted);

    QPointer<AudioController> self(this);
    std::thread([self, isMuted]() {
        if (hardware::AudioUtils::is_muted() != isMuted) {
            hardware::AudioUtils::toggle_mute(/*persist=*/true);
        }
        const bool actualMuted = hardware::AudioUtils::is_muted();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, actualMuted]() {
            if (!self) return;
            if (self->m_muted != actualMuted) {
                self->m_muted = actualMuted;
                emit self->mutedChanged();
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void AudioController::setOutputDevice(int index) {
    if (index < 0 || index >= m_outputDevices.size()) return;
    m_currentOutputIndex = index;
    emit outputDeviceChanged();
    emit toastRequested(QStringLiteral("Audio output routed to: ") + m_outputDevices.at(index), false);
    emit settingModified(QStringLiteral("output_device_index"), index);

    QPointer<AudioController> self(this);
    std::thread([self, index]() {
        hardware::AudioUtils::set_output_device_by_index(index);
        const int curVol = hardware::AudioUtils::get_volume_percent();
        const bool curMuted = hardware::AudioUtils::is_muted();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, curVol, curMuted]() {
            if (!self) return;
            if (self->m_volume != curVol) { self->m_volume = curVol; emit self->volumeChanged(); }
            if (self->m_muted != curMuted) { self->m_muted = curMuted; emit self->mutedChanged(); }
        }, Qt::QueuedConnection);
    }).detach();
}

void AudioController::setInputVolume(int vol) {
    vol = std::clamp(vol, 0, 100);
    if (m_inputVolume != vol) {
        m_inputVolume = vol;
        emit inputVolumeChanged();
    }
    std::thread([vol]() {
        hardware::AudioUtils::set_input_volume_percent(vol);
    }).detach();
}

void AudioController::setInputMuted(bool isMuted) {
    if (m_inputMuted == isMuted) return;
    m_inputMuted = isMuted;
    emit inputMutedChanged();
    QPointer<AudioController> self(this);
    std::thread([self, isMuted]() {
        if (hardware::AudioUtils::is_input_muted() != isMuted) {
            hardware::AudioUtils::toggle_input_mute();
        }
        const bool actual = hardware::AudioUtils::is_input_muted();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, actual]() {
            if (self && self->m_inputMuted != actual) {
                self->m_inputMuted = actual;
                emit self->inputMutedChanged();
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void AudioController::setInputDevice(int index) {
    if (index < 0 || index >= m_inputDevices.size()) return;
    m_currentInputIndex = index;
    emit inputDeviceChanged();
    emit toastRequested(QStringLiteral("Input source: ") + m_inputDevices.at(index), false);
    std::thread([index]() {
        hardware::AudioUtils::set_input_device_by_index(index);
    }).detach();
}

void AudioController::refreshAudioDevices() {
    QPointer<AudioController> self(this);
    std::thread([self]() {
        auto outDevs = hardware::AudioUtils::get_output_devices();
        int outIdx = hardware::AudioUtils::get_current_output_device_index();
        auto inDevs = hardware::AudioUtils::get_input_devices();
        int inIdx = hardware::AudioUtils::get_current_input_device_index();

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, outDevs, outIdx, inDevs, inIdx]() {
            if (!self) return;
            QStringList outList;
            for (const auto& d : outDevs) outList.append(QString::fromStdString(d));
            if (outList.isEmpty()) outList.append(QStringLiteral("Default Audio Output"));
            self->m_outputDevices = outList;
            self->m_currentOutputIndex = std::clamp(outIdx, 0, static_cast<int>(outList.size() - 1));
            emit self->outputDevicesChanged();
            emit self->outputDeviceChanged();

            QStringList inList;
            for (const auto& d : inDevs) inList.append(QString::fromStdString(d));
            if (inList.isEmpty()) inList.append(QStringLiteral("No Audio Input Devices Available"));
            self->m_inputDevices = inList;
            self->m_currentInputIndex = std::clamp(inIdx, 0, static_cast<int>(inList.size() - 1));
            emit self->inputDevicesChanged();
            emit self->inputDeviceChanged();
        }, Qt::QueuedConnection);
    }).detach();
}

void AudioController::syncAudioState() {
    if (m_audioBusy->exchange(true)) return;
    QPointer<AudioController> self(this);
    auto busy = m_audioBusy;
    std::thread([self, busy]() {
        const int curVol = hardware::AudioUtils::get_volume_percent();
        const bool curMuted = hardware::AudioUtils::is_muted();
        const int curInVol = hardware::AudioUtils::get_input_volume_percent();
        const bool curInMuted = hardware::AudioUtils::is_input_muted();
        busy->store(false);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, curVol, curMuted, curInVol, curInMuted]() {
            if (!self) return;
            if (self->m_pendingVolume < 0) {
                if (self->m_volume != curVol) { self->m_volume = curVol; emit self->volumeChanged(); }
                if (self->m_muted != curMuted) { self->m_muted = curMuted; emit self->mutedChanged(); }
            }
            if (self->m_inputVolume != curInVol) { self->m_inputVolume = curInVol; emit self->inputVolumeChanged(); }
            if (self->m_inputMuted != curInMuted) { self->m_inputMuted = curInMuted; emit self->inputMutedChanged(); }
        }, Qt::QueuedConnection);
    }).detach();
}

void AudioController::playTestSound() {
    hardware::AudioUtils::play_chime();
    emit toastRequested(QStringLiteral("Playing system test sound..."), false);
}

} // namespace tinexus::settings_ui
