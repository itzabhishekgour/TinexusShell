// ============================================================================
// DockIpcClient.hpp — D-Bus Notification Badges & IPC Client (Slice 7)
// Ref: Architecture Blueprint §6, docs/05_UI_UX_GUIDELINES.md
// Subscribes to io.tinexus.shell.Notifications.BadgeCountChanged signal.
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace tinexus::dock {

class DockModel;

class DockIpcClient : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStatusChanged)

public:
    explicit DockIpcClient(DockModel* model = nullptr, QObject* parent = nullptr);
    ~DockIpcClient() override;

    bool init();
    void shutdown();

    [[nodiscard]] bool isConnected() const { return m_connected; }
    void setModel(DockModel* model) { m_model = model; }

    // Test & Simulation API
    Q_INVOKABLE void simulateBadgeCount(const QString& appId, uint32_t count);

signals:
    void connectionStatusChanged(bool connected);
    void badgeCountReceived(const QString& appId, uint32_t count);

public slots:
    void onBadgeCountChanged(const QString& appId, uint count);

private:
    DockModel* m_model{nullptr};
    bool       m_connected{false};
};

} // namespace tinexus::dock
