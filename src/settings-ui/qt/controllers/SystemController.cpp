#include "SystemController.hpp"
#include <common/DBusNames.hpp>
#include <common/logger.hpp>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusVariant>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace tinexus::settings_ui {

SystemController::SystemController(QObject* parent)
    : QObject(parent)
{
    refreshPowerStatus();
}

void SystemController::refreshPowerStatus() {
    QPointer<SystemController> self(this);
    std::thread([self]() {
        int profileIdx = 1;
        if (fs::exists("/usr/bin/powerprofilesctl")) {
            QProcess p;
            p.start(QStringLiteral("/usr/bin/powerprofilesctl"), {QStringLiteral("get")});
            if (p.waitForFinished(1200) && p.exitCode() == 0) {
                QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
                if (out == QStringLiteral("power-saver")) profileIdx = 0;
                else if (out == QStringLiteral("performance")) profileIdx = 2;
                else profileIdx = 1;
            }
        } else {
            std::ifstream pp("/sys/firmware/acpi/platform_profile");
            if (pp.is_open()) {
                std::string mode;
                pp >> mode;
                if (mode == "low-power" || mode == "quiet") profileIdx = 0;
                else if (mode == "performance") profileIdx = 2;
                else profileIdx = 1;
            }
        }

        bool hasBat = false;
        int pct = 100;
        bool onBat = false;
        QString stateStr = QStringLiteral("AC Power");

        // 1. Try UPower D-Bus DisplayDevice first
        QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"),
                              QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::systemBus());
        if (upower.isValid()) {
            QDBusReply<QDBusVariant> present = upower.call(QStringLiteral("Get"),
                                                           QStringLiteral("org.freedesktop.UPower.Device"),
                                                           QStringLiteral("IsPresent"));
            if (present.isValid() && present.value().variant().toBool()) {
                QDBusReply<QDBusVariant> typeR = upower.call(QStringLiteral("Get"),
                                                             QStringLiteral("org.freedesktop.UPower.Device"),
                                                             QStringLiteral("Type"));
                if (!typeR.isValid() || typeR.value().variant().toUInt() == 2) {
                    hasBat = true;
                    QDBusReply<QDBusVariant> pctR = upower.call(QStringLiteral("Get"),
                                                                QStringLiteral("org.freedesktop.UPower.Device"),
                                                                QStringLiteral("Percentage"));
                    if (pctR.isValid()) pct = std::clamp(static_cast<int>(pctR.value().variant().toDouble() + 0.5), 0, 100);
                    QDBusReply<QDBusVariant> stR = upower.call(QStringLiteral("Get"),
                                                               QStringLiteral("org.freedesktop.UPower.Device"),
                                                               QStringLiteral("State"));
                    const uint st = stR.isValid() ? stR.value().variant().toUInt() : 0;
                    if (st == 1) { stateStr = QStringLiteral("Charging"); onBat = false; }
                    else if (st == 2) { stateStr = QStringLiteral("Discharging"); onBat = true; }
                    else if (st == 4) { stateStr = QStringLiteral("Fully Charged"); onBat = false; }
                    else { stateStr = QStringLiteral("Connected to Power"); }
                }
            }
        }

        // 2. Fallback to /sys/class/power_supply/*
        if (!hasBat && fs::exists("/sys/class/power_supply")) {
            try {
                for (const auto& e : fs::directory_iterator("/sys/class/power_supply")) {
                    std::ifstream tf(e.path() / "type");
                    std::string ptype;
                    if (!(tf >> ptype) || ptype != "Battery") continue;
                    hasBat = true;
                    std::ifstream cf(e.path() / "capacity");
                    if (cf.is_open()) {
                        int v = 100;
                        if (cf >> v) pct = std::clamp(v, 0, 100);
                    }
                    std::ifstream sf(e.path() / "status");
                    std::string st;
                    if (std::getline(sf, st)) {
                        while (!st.empty() && (st.back() == '\n' || st.back() == '\r')) st.pop_back();
                        if (!st.empty()) {
                            stateStr = QString::fromStdString(st);
                            onBat = (st == "Discharging");
                        }
                    }
                    break;
                }
            } catch (...) {}
        }

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, profileIdx, hasBat, pct, stateStr, onBat]() {
            if (!self) return;
            self->m_powerProfileIndex = profileIdx;
            self->m_hasBattery = hasBat;
            self->m_batteryPercent = pct;
            self->m_batteryState = stateStr;
            self->m_onBattery = onBat;
            emit self->powerProfileIndexChanged();
            emit self->batteryStatusChanged();
        }, Qt::QueuedConnection);
    }).detach();
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
        const char* ctl_modes[] = {"power-saver", "balanced", "performance"};

        if (fs::exists("/usr/bin/powerprofilesctl")) {
            QProcess::startDetached(QStringLiteral("/usr/bin/powerprofilesctl"),
                                    {QStringLiteral("set"), QString::fromLatin1(ctl_modes[index])});
        }
        std::ofstream pp("/sys/firmware/acpi/platform_profile");
        if (pp.is_open()) {
            pp << sys_modes[index];
        }

        emit powerProfileIndexChanged();
        emit toastRequested(QStringLiteral("Energy mode set to %1").arg(QString::fromLatin1(profiles[index])), false);
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
    if (auto* cb = QGuiApplication::clipboard()) {
        cb->clear();
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(
        tinexus::common::dbus::qservice::Clipboard(),
        tinexus::common::dbus::qpath::Clipboard(),
        tinexus::common::dbus::qinterface::Clipboard(),
        QStringLiteral("ClearHistory"));
    QDBusConnection::sessionBus().send(msg);
    emit toastRequested(QStringLiteral("Clipboard history cleared"), false);
}

void SystemController::sessionLock() {
    if (fs::exists("/usr/bin/tinexus-lock")) {
        QProcess::startDetached(QStringLiteral("/usr/bin/tinexus-lock"), {});
    }
}

void SystemController::sessionSuspend() {
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage r = login1.call(QStringLiteral("Suspend"), false);
        if (r.type() != QDBusMessage::ErrorMessage) return;
    }
    QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("suspend")});
}

void SystemController::sessionReboot() {
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage r = login1.call(QStringLiteral("Reboot"), false);
        if (r.type() != QDBusMessage::ErrorMessage) return;
    }
    if (!QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("/bin/systemctl"), QStringLiteral("reboot")})) {
        sync();
        ::reboot(RB_AUTOBOOT);
    }
}

void SystemController::sessionShutdown() {
    QDBusInterface login1(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusMessage r = login1.call(QStringLiteral("PowerOff"), false);
        if (r.type() != QDBusMessage::ErrorMessage) return;
    }
    if (!QProcess::startDetached(QStringLiteral("sudo"), {QStringLiteral("/bin/systemctl"), QStringLiteral("poweroff")})) {
        sync();
        ::reboot(RB_POWER_OFF);
    }
}

} // namespace tinexus::settings_ui
