// ============================================================================
// DockAdaptor.hpp — Qt6 D-Bus Adaptor for io.tinexus.Dock
// ============================================================================
#pragma once

#include <QtDBus/QDBusAbstractAdaptor>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>

namespace tinexus::dock {

class DockBridge;

class DockAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.tinexus.Dock")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"io.tinexus.Dock\">\n"
"    <property name=\"RunningApps\" type=\"as\" access=\"read\"/>\n"
"    <property name=\"FocusedApp\" type=\"s\" access=\"read\"/>\n"
"    <property name=\"BadgeCount\" type=\"u\" access=\"read\"/>\n"
"    <property name=\"BadgeCounts\" type=\"a{su}\" access=\"read\"/>\n"
"    <method name=\"QueryIconPosition\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"x\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"y\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"w\"/>\n"
"      <arg direction=\"out\" type=\"i\" name=\"h\"/>\n"
"    </method>\n"
"    <method name=\"NotifyMinimized\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"t\" name=\"surfaceId\"/>\n"
"    </method>\n"
"    <method name=\"NotifyRestored\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"t\" name=\"surfaceId\"/>\n"
"    </method>\n"
"    <method name=\"RaiseAndFocus\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"    </method>\n"
"    <method name=\"NotifyFocusChanged\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"b\" name=\"isFocused\"/>\n"
"    </method>\n"
"    <method name=\"NotifyAppStarted\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"t\" name=\"surfaceId\"/>\n"
"    </method>\n"
"    <method name=\"NotifyAppClosed\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"t\" name=\"surfaceId\"/>\n"
"    </method>\n"
"    <method name=\"RestoreWindow\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"    </method>\n"
"    <method name=\"SetBadgeCount\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"in\" type=\"u\" name=\"count\"/>\n"
"    </method>\n"
"    <method name=\"GetBadgeCount\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"appId\"/>\n"
"      <arg direction=\"out\" type=\"u\" name=\"count\"/>\n"
"    </method>\n"
"    <signal name=\"AppStateChanged\">\n"
"      <arg type=\"s\" name=\"appId\"/>\n"
"      <arg type=\"i\" name=\"state\"/>\n"
"    </signal>\n"
"    <signal name=\"BadgeCountChanged\">\n"
"      <arg type=\"s\" name=\"appId\"/>\n"
"      <arg type=\"u\" name=\"count\"/>\n"
"    </signal>\n"
"  </interface>\n"
"")

    Q_PROPERTY(QStringList RunningApps READ runningApps)
    Q_PROPERTY(QString FocusedApp READ focusedApp)
    Q_PROPERTY(uint BadgeCount READ badgeCount)
    Q_PROPERTY(QVariantMap BadgeCounts READ badgeCounts)

public:
    explicit DockAdaptor(DockBridge* parent);
    ~DockAdaptor() override = default;

    QStringList runningApps() const;
    QString focusedApp() const;
    uint badgeCount() const;
    QVariantMap badgeCounts() const;

public slots:
    void QueryIconPosition(const QString& appId, int& x, int& y, int& w, int& h);
    void NotifyMinimized(const QString& appId, qulonglong surfaceId);
    void NotifyRestored(const QString& appId, qulonglong surfaceId);
    void RaiseAndFocus(const QString& appId);
    void NotifyFocusChanged(const QString& appId, bool isFocused);
    void NotifyAppStarted(const QString& appId, qulonglong surfaceId);
    void NotifyAppClosed(const QString& appId, qulonglong surfaceId);
    void RestoreWindow(const QString& appId);
    void SetBadgeCount(const QString& appId, uint count);
    uint GetBadgeCount(const QString& appId);

signals:
    void AppStateChanged(const QString& appId, int state);
    void BadgeCountChanged(const QString& appId, uint count);

private:
    DockBridge* m_bridge;
};

} // namespace tinexus::dock
