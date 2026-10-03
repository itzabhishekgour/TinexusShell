// ============================================================================
// DockIpcClient.cpp — D-Bus Notification Badges & IPC Client (Slice 7)
// Ref: Architecture Blueprint §6, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DockIpcClient.hpp"
#include "dock/DockModel.hpp"
#include <common/logger.hpp>
#include <common/DBusNames.hpp>

#if __has_include(<QtDBus/QDBusConnection>)
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#define HAVE_QT_DBUS 1
#else
#define HAVE_QT_DBUS 0
#endif

namespace tinexus::dock {

DockIpcClient::DockIpcClient(DockModel* model, QObject* parent)
    : QObject(parent), m_model(model)
{
}

DockIpcClient::~DockIpcClient() {
    shutdown();
}

bool DockIpcClient::init() {
#if HAVE_QT_DBUS
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        tinexus::log::warn("[DockIpcClient] Session D-Bus is not connected — running in fallback mode");
        m_connected = false;
        emit connectionStatusChanged(false);
        return false;
    }

    // Subscribe to Notifications.BadgeCountChanged(QString appId, uint count)
    bool ok = bus.connect(
        tinexus::common::dbus::qservice::Notifications(),
        tinexus::common::dbus::qpath::Notifications(),
        tinexus::common::dbus::qinterface::Notifications(),
        QStringLiteral("BadgeCountChanged"),
        this,
        SLOT(onBadgeCountChanged(QString, uint))
    );

    if (ok) {
        m_connected = true;
        tinexus::log::info("[DockIpcClient] Subscribed to Notifications.BadgeCountChanged signal");
        emit connectionStatusChanged(true);
        return true;
    } else {
        tinexus::log::warn("[DockIpcClient] Failed to register D-Bus signal hook for BadgeCountChanged");
        m_connected = false;
        emit connectionStatusChanged(false);
        return false;
    }
#else
    tinexus::log::info("[DockIpcClient] QtDBus not compiled; running in simulation mode");
    m_connected = false;
    return false;
#endif
}

void DockIpcClient::shutdown() {
#if HAVE_QT_DBUS
    if (m_connected) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (bus.isConnected()) {
            bus.disconnect(
                tinexus::common::dbus::qservice::Notifications(),
                tinexus::common::dbus::qpath::Notifications(),
                tinexus::common::dbus::qinterface::Notifications(),
                QStringLiteral("BadgeCountChanged"),
                this,
                SLOT(onBadgeCountChanged(QString, uint))
            );
        }
        m_connected = false;
        emit connectionStatusChanged(false);
    }
#endif
}

void DockIpcClient::onBadgeCountChanged(const QString& appId, uint count) {
    tinexus::log::info("[DockIpcClient] Received badge update: appId='{}' count={}",
                       appId.toStdString(), count);

    if (m_model) {
        m_model->setBadgeCount(appId, static_cast<uint32_t>(count));
    }

    emit badgeCountReceived(appId, static_cast<uint32_t>(count));
}

void DockIpcClient::simulateBadgeCount(const QString& appId, uint32_t count) {
    tinexus::log::info("[DockIpcClient] Simulating badge update: appId='{}' count={}",
                       appId.toStdString(), count);
    onBadgeCountChanged(appId, static_cast<uint>(count));
}

} // namespace tinexus::dock

#include "moc_DockIpcClient.cpp"
