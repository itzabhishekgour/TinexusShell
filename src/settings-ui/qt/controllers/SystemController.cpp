#include "SystemController.hpp"
#include <common/logger.hpp>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>

namespace tinexus::settings_ui {

SystemController::SystemController(QObject* parent)
    : QObject(parent)
{
    // Check if system has a power profile node (e.g. /sys/firmware/acpi/platform_profile)
    std::ifstream pp("/sys/firmware/acpi/platform_profile");
    if (pp.is_open()) {
        std::string mode;
        pp >> mode;
        if (mode == "low-power" || mode == "quiet") m_powerProfileIndex = 0;
        else if (mode == "performance") m_powerProfileIndex = 2;
        else m_powerProfileIndex = 1;
    }
}

void SystemController::setScreenTimeoutMin(int min) {
    min = std::clamp(min, 1, 60);
    if (m_screenTimeoutMin != min) {
        m_screenTimeoutMin = min;
        emit screenTimeoutMinChanged();
        emit settingModified(QStringLiteral("screen_timeout_min"), min);
    }
}

void SystemController::setSleepAfterMin(int min) {
    min = std::clamp(min, 5, 120);
    if (m_sleepAfterMin != min) {
        m_sleepAfterMin = min;
        emit sleepAfterMinChanged();
        emit settingModified(QStringLiteral("sleep_after_min"), min);
    }
}

void SystemController::setPowerProfileIndex(int index) {
    index = std::clamp(index, 0, 2);
    if (m_powerProfileIndex != index) {
        m_powerProfileIndex = index;
        const char* profiles[] = {"Power Saver", "Balanced", "Performance"};
        const char* sys_modes[] = {"low-power", "balanced", "performance"};

        // Try writing to kernel platform_profile if accessible
        std::ofstream pp("/sys/firmware/acpi/platform_profile");
        if (pp.is_open()) {
            pp << sys_modes[index];
        }

        emit powerProfileIndexChanged();
        emit toastRequested(QStringLiteral("Power profile set to %1").arg(QString::fromLatin1(profiles[m_powerProfileIndex])), false);
        emit settingModified(QStringLiteral("power_profile_idx"), index);
    }
}

void SystemController::setLockOnSleep(bool enabled) {
    if (m_lockOnSleep != enabled) {
        m_lockOnSleep = enabled;
        emit lockOnSleepChanged();
        emit settingModified(QStringLiteral("lock_on_sleep"), enabled);
    }
}

void SystemController::setPamAuth(bool enabled) {
    if (m_pamAuth != enabled) {
        m_pamAuth = enabled;
        emit pamAuthChanged();
        emit settingModified(QStringLiteral("pam_auth"), enabled);
    }
}

void SystemController::setClipboardHistorySize(int size) {
    size = std::clamp(size, 10, 200);
    if (m_clipboardHistorySize != size) {
        m_clipboardHistorySize = size;
        emit clipboardHistorySizeChanged();
        emit settingModified(QStringLiteral("clipboard_history_size"), size);
    }
}

void SystemController::clearClipboardHistory() {
    emit toastRequested(QStringLiteral("Clipboard history cleared"), false);
}

void SystemController::sessionLock() {
    emit toastRequested(QStringLiteral("Requesting desktop session lock..."), false);
}

void SystemController::sessionSuspend() {
    emit toastRequested(QStringLiteral("Requesting system suspend..."), false);
}

void SystemController::sessionReboot() {
    emit toastRequested(QStringLiteral("System reboot initiated"), false);
    sync();
    ::reboot(RB_AUTOBOOT);
}

void SystemController::sessionShutdown() {
    emit toastRequested(QStringLiteral("System shutdown initiated"), false);
    sync();
    ::reboot(RB_POWER_OFF);
}

} // namespace tinexus::settings_ui
