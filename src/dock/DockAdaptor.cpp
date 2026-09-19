// ============================================================================
// DockAdaptor.cpp — Qt6 D-Bus Adaptor implementation for io.tinexus.Dock
// ============================================================================
#include "dock/DockAdaptor.hpp"
#include "dock/DockBridge.hpp"

namespace tinexus::dock {

DockAdaptor::DockAdaptor(DockBridge* parent)
    : QDBusAbstractAdaptor(parent), m_bridge(parent)
{
    setAutoRelaySignals(true);
}

QStringList DockAdaptor::runningApps() const {
    return m_bridge ? m_bridge->runningApps() : QStringList{};
}

QString DockAdaptor::focusedApp() const {
    return m_bridge ? m_bridge->focusedApp() : QString{};
}

uint DockAdaptor::badgeCount() const {
    return m_bridge ? m_bridge->totalBadgeCount() : 0u;
}

QVariantMap DockAdaptor::badgeCounts() const {
    return m_bridge ? m_bridge->badgeCounts() : QVariantMap{};
}

void DockAdaptor::QueryIconPosition(const QString& appId, int& x, int& y, int& w, int& h) {
    if (m_bridge) {
        m_bridge->queryIconPosition(appId, x, y, w, h);
    } else {
        x = y = w = h = 0;
    }
}

void DockAdaptor::NotifyMinimized(const QString& appId, qulonglong surfaceId) {
    if (m_bridge) m_bridge->onDBusNotifyMinimized(appId, surfaceId);
}

void DockAdaptor::NotifyRestored(const QString& appId, qulonglong surfaceId) {
    if (m_bridge) m_bridge->onDBusNotifyRestored(appId, surfaceId);
}

void DockAdaptor::RaiseAndFocus(const QString& appId) {
    if (m_bridge) m_bridge->raiseApp(appId);
}

void DockAdaptor::NotifyFocusChanged(const QString& appId, bool isFocused) {
    if (m_bridge) m_bridge->onDBusNotifyFocusChanged(appId, isFocused);
}

void DockAdaptor::NotifyAppStarted(const QString& appId, qulonglong surfaceId) {
    if (m_bridge) m_bridge->onDBusNotifyAppStarted(appId, surfaceId);
}

void DockAdaptor::NotifyAppClosed(const QString& appId, qulonglong surfaceId) {
    if (m_bridge) m_bridge->onDBusNotifyAppClosed(appId, surfaceId);
}

void DockAdaptor::RestoreWindow(const QString& appId) {
    if (m_bridge) m_bridge->restoreWindow(appId);
}

void DockAdaptor::SetBadgeCount(const QString& appId, uint count) {
    if (m_bridge) {
        m_bridge->setBadgeCount(appId, count);
        emit BadgeCountChanged(appId, count);
    }
}

uint DockAdaptor::GetBadgeCount(const QString& appId) {
    return m_bridge ? m_bridge->badgeCount(appId) : 0u;
}

void DockAdaptor::PinApp(const QString& appId) {
    if (m_bridge) m_bridge->pinApp(appId);
}

void DockAdaptor::UnpinApp(const QString& appId) {
    if (m_bridge) m_bridge->unpinApp(appId);
}

void DockAdaptor::RequestAttention(const QString& appId) {
    if (m_bridge) m_bridge->requestAttention(appId);
}

void DockAdaptor::MoveItem(int fromIndex, int toIndex) {
    if (m_bridge) m_bridge->moveItem(fromIndex, toIndex);
}

void DockAdaptor::EmptyTrash() {
    if (m_bridge) m_bridge->emptyTrash();
}

void DockAdaptor::LaunchWithUris(const QString& appId, const QStringList& uris) {
    if (m_bridge) m_bridge->launchWithUris(appId, uris);
}

void DockAdaptor::SpawnApp(const QString& appId) {
    if (m_bridge) m_bridge->spawnApp(appId);
}

void DockAdaptor::ForceQuitApp(const QString& appId) {
    if (m_bridge) m_bridge->forceQuitApp(appId);
}

} // namespace tinexus::dock

#include "moc_DockAdaptor.cpp"
