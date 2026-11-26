#include "BluetoothController.hpp"
#include <common/logger.hpp>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusVariant>
#include <QDBusArgument>
#include <filesystem>
#include <thread>

namespace tinexus::settings_ui {

namespace {
QString iconForBluezClass(const QString& icon, const QString& name) {
    const QString lower = icon.toLower() + QLatin1Char(' ') + name.toLower();
    if (lower.contains(QStringLiteral("headset")) || lower.contains(QStringLiteral("headphone")) || lower.contains(QStringLiteral("audio"))) {
        return QStringLiteral("audio-headphones");
    }
    if (lower.contains(QStringLiteral("mouse"))) return QStringLiteral("input-mouse");
    if (lower.contains(QStringLiteral("keyboard"))) return QStringLiteral("input-keyboard");
    if (lower.contains(QStringLiteral("phone"))) return QStringLiteral("phone");
    return QStringLiteral("bluetooth");
}

QString runBtctl(const QStringList& args, int timeoutMs = 4000) {
    if (!std::filesystem::exists("/usr/bin/bluetoothctl")) return {};
    QProcess p;
    p.start(QStringLiteral("/usr/bin/bluetoothctl"), args);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(200);
        return {};
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}
} // namespace

BluetoothController::BluetoothController(QObject* parent)
    : QObject(parent)
{
    QTimer::singleShot(0, this, &BluetoothController::refreshDevices);
}

void BluetoothController::refreshDevices() {
    if (m_busy->exchange(true)) return;
    QPointer<BluetoothController> self(this);
    auto busy = m_busy;
    std::thread([self, busy]() {
        bool avail = false;
        bool powered = false;
        bool discovering = false;
        QString adapterName = QStringLiteral("Bluetooth Adapter");
        QString adapterPath;
        QVariantList devs;

        // Query BlueZ D-Bus ObjectManager via clean bluetoothctl fallback if D-Bus interface unavailable
        QString ctlShow = runBtctl({QStringLiteral("show")}, 2000);
        if (!ctlShow.isEmpty() && !ctlShow.contains(QStringLiteral("No default controller available"))) {
            avail = true;
            for (const QString& line : ctlShow.split(QLatin1Char('\n'))) {
                const QString t = line.trimmed();
                if (t.startsWith(QStringLiteral("Name:"))) {
                    adapterName = t.mid(5).trimmed();
                } else if (t.startsWith(QStringLiteral("Powered:"))) {
                    powered = t.mid(8).trimmed().compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
                } else if (t.startsWith(QStringLiteral("Discovering:"))) {
                    discovering = t.mid(12).trimmed().compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
                }
            }
        }

        // Also confirm via org.bluez.Adapter1 D-Bus property check
        QDBusInterface adapterProp(QStringLiteral("org.bluez"),
                                   QStringLiteral("/org/bluez/hci0"),
                                   QStringLiteral("org.freedesktop.DBus.Properties"),
                                   QDBusConnection::systemBus());
        if (adapterProp.isValid()) {
            avail = true;
            adapterPath = QStringLiteral("/org/bluez/hci0");
            QDBusReply<QDBusVariant> pwr = adapterProp.call(QStringLiteral("Get"), QStringLiteral("org.bluez.Adapter1"), QStringLiteral("Powered"));
            if (pwr.isValid()) powered = pwr.value().variant().toBool();
            QDBusReply<QDBusVariant> disc = adapterProp.call(QStringLiteral("Get"), QStringLiteral("org.bluez.Adapter1"), QStringLiteral("Discovering"));
            if (disc.isValid()) discovering = disc.value().variant().toBool();
            QDBusReply<QDBusVariant> alias = adapterProp.call(QStringLiteral("Get"), QStringLiteral("org.bluez.Adapter1"), QStringLiteral("Alias"));
            if (alias.isValid() && !alias.value().variant().toString().isEmpty()) {
                adapterName = alias.value().variant().toString();
            }
        }

        if (avail) {
            QString devLines = runBtctl({QStringLiteral("devices")}, 2000);
            for (const QString& line : devLines.split(QLatin1Char('\n'))) {
                const QString t = line.trimmed();
                if (!t.startsWith(QStringLiteral("Device "))) continue;
                const QString rest = t.mid(7);
                const int sp = rest.indexOf(QLatin1Char(' '));
                if (sp <= 0) continue;
                const QString addr = rest.left(sp).trimmed();
                QString name = rest.mid(sp + 1).trimmed();
                if (name.isEmpty()) name = addr;

                bool paired = false;
                bool connected = false;
                bool trusted = false;
                QString iconHint;
                QString info = runBtctl({QStringLiteral("info"), addr}, 1500);
                for (const QString& il : info.split(QLatin1Char('\n'))) {
                    const QString it = il.trimmed();
                    if (it.startsWith(QStringLiteral("Paired:"))) paired = it.endsWith(QStringLiteral("yes"));
                    else if (it.startsWith(QStringLiteral("Connected:"))) connected = it.endsWith(QStringLiteral("yes"));
                    else if (it.startsWith(QStringLiteral("Trusted:"))) trusted = it.endsWith(QStringLiteral("yes"));
                    else if (it.startsWith(QStringLiteral("Icon:"))) iconHint = it.mid(5).trimmed();
                    else if (it.startsWith(QStringLiteral("Alias:"))) {
                        const QString al = it.mid(6).trimmed();
                        if (!al.isEmpty()) name = al;
                    }
                }

                QVariantMap d;
                d[QStringLiteral("address")] = addr;
                d[QStringLiteral("name")] = name;
                d[QStringLiteral("paired")] = paired;
                d[QStringLiteral("connected")] = connected;
                d[QStringLiteral("trusted")] = trusted;
                d[QStringLiteral("iconName")] = iconForBluezClass(iconHint, name);
                devs.append(d);
            }
        }

        busy->store(false);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, avail, powered, discovering, adapterName, adapterPath, devs]() {
            if (!self) return;
            self->m_available = avail;
            self->m_enabled = powered;
            self->m_scanning = discovering;
            self->m_adapterName = adapterName;
            self->m_adapterPath = adapterPath;
            self->m_devices = devs;
            emit self->bluetoothStateChanged();
            emit self->devicesChanged();
        }, Qt::QueuedConnection);
    }).detach();
}

void BluetoothController::setBluetoothEnabled(bool enabled) {
    m_enabled = enabled;
    emit bluetoothStateChanged();
    QPointer<BluetoothController> self(this);
    std::thread([self, enabled]() {
        bool ok = false;
        QDBusInterface prop(QStringLiteral("org.bluez"),
                            QStringLiteral("/org/bluez/hci0"),
                            QStringLiteral("org.freedesktop.DBus.Properties"),
                            QDBusConnection::systemBus());
        if (prop.isValid()) {
            QDBusMessage reply = prop.call(QStringLiteral("Set"),
                                           QStringLiteral("org.bluez.Adapter1"),
                                           QStringLiteral("Powered"),
                                           QVariant::fromValue(QDBusVariant(enabled)));
            ok = (reply.type() != QDBusMessage::ErrorMessage);
        }
        if (!ok) {
            runBtctl({QStringLiteral("power"), enabled ? QStringLiteral("on") : QStringLiteral("off")}, 2500);
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, enabled]() {
            if (!self) return;
            emit self->toastRequested(enabled ? QStringLiteral("Bluetooth enabled") : QStringLiteral("Bluetooth disabled"), false);
            self->refreshDevices();
        }, Qt::QueuedConnection);
    }).detach();
}

void BluetoothController::startDiscovery() {
    if (m_scanning) return;
    m_scanning = true;
    emit bluetoothStateChanged();
    QPointer<BluetoothController> self(this);
    std::thread([self]() {
        runBtctl({QStringLiteral("--timeout"), QStringLiteral("5"), QStringLiteral("scan"), QStringLiteral("on")}, 6500);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->m_scanning = false;
            emit self->bluetoothStateChanged();
            self->refreshDevices();
        }, Qt::QueuedConnection);
    }).detach();
}

void BluetoothController::stopDiscovery() {
    QDBusInterface adapter(QStringLiteral("org.bluez"),
                           QStringLiteral("/org/bluez/hci0"),
                           QStringLiteral("org.bluez.Adapter1"),
                           QDBusConnection::systemBus());
    if (adapter.isValid()) {
        adapter.call(QDBus::NoBlock, QStringLiteral("StopDiscovery"));
    }
    m_scanning = false;
    emit bluetoothStateChanged();
}

void BluetoothController::pairAndConnect(const QString& address) {
    if (address.isEmpty()) return;
    emit toastRequested(QStringLiteral("Pairing and connecting %1...").arg(address), false);
    QPointer<BluetoothController> self(this);
    std::thread([self, address]() {
        runBtctl({QStringLiteral("pair"), address}, 8000);
        runBtctl({QStringLiteral("trust"), address}, 2000);
        QString out = runBtctl({QStringLiteral("connect"), address}, 8000);
        const bool ok = out.contains(QStringLiteral("Connection successful"), Qt::CaseInsensitive);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, address, ok]() {
            if (!self) return;
            emit self->toastRequested(ok ? QStringLiteral("Connected to %1").arg(address)
                                         : QStringLiteral("Could not connect to %1").arg(address), !ok);
            self->refreshDevices();
        }, Qt::QueuedConnection);
    }).detach();
}

void BluetoothController::disconnectDevice(const QString& address) {
    if (address.isEmpty()) return;
    QPointer<BluetoothController> self(this);
    std::thread([self, address]() {
        runBtctl({QStringLiteral("disconnect"), address}, 4000);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, address]() {
            if (!self) return;
            emit self->toastRequested(QStringLiteral("Disconnected %1").arg(address), false);
            self->refreshDevices();
        }, Qt::QueuedConnection);
    }).detach();
}

void BluetoothController::removeDevice(const QString& address) {
    if (address.isEmpty()) return;
    QPointer<BluetoothController> self(this);
    std::thread([self, address]() {
        runBtctl({QStringLiteral("remove"), address}, 4000);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, address]() {
            if (!self) return;
            emit self->toastRequested(QStringLiteral("Removed %1").arg(address), false);
            self->refreshDevices();
        }, Qt::QueuedConnection);
    }).detach();
}

} // namespace tinexus::settings_ui
