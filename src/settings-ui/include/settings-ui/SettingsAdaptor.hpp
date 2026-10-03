// ============================================================================
// SettingsAdaptor.hpp — Qt6 D-Bus Adaptor for io.tinexus.shell.Settings
// ============================================================================
#pragma once

#include <common/DBusNames.hpp>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusVariant>
#include <QtCore/QString>
#include <QtCore/QVariantMap>

namespace tinexus::settings_ui {

class SettingsBridge;

class SettingsAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", TINEXUS_DBUS_INTERFACE_SETTINGS)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"" TINEXUS_DBUS_INTERFACE_SETTINGS "\">\n"
"    <property name=\"CurrentTheme\" type=\"s\" access=\"readwrite\"/>\n"
"    <property name=\"Language\" type=\"s\" access=\"readwrite\"/>\n"
"    <method name=\"GetValue\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"key\"/>\n"
"      <arg direction=\"out\" type=\"v\" name=\"value\"/>\n"
"    </method>\n"
"    <method name=\"SetValue\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"key\"/>\n"
"      <arg direction=\"in\" type=\"v\" name=\"value\"/>\n"
"      <arg direction=\"out\" type=\"b\" name=\"ok\"/>\n"
"    </method>\n"
"    <method name=\"GetAllSettings\">\n"
"      <arg direction=\"out\" type=\"a{sv}\" name=\"settings\"/>\n"
"    </method>\n"
"    <method name=\"ReloadConfig\">\n"
"    </method>\n"
"    <signal name=\"ConfigChanged\">\n"
"      <arg type=\"s\" name=\"key\"/>\n"
"      <arg type=\"v\" name=\"value\"/>\n"
"    </signal>\n"
"    <signal name=\"ThemeChanged\">\n"
"      <arg type=\"s\" name=\"themeName\"/>\n"
"    </signal>\n"
"  </interface>\n"
"")

    Q_PROPERTY(QString CurrentTheme READ currentTheme WRITE setCurrentTheme)
    Q_PROPERTY(QString Language READ language WRITE setLanguage)

public:
    explicit SettingsAdaptor(SettingsBridge* parent);
    ~SettingsAdaptor() override = default;

    QString currentTheme() const;
    void setCurrentTheme(const QString& theme);

    QString language() const;
    void setLanguage(const QString& lang);

public slots:
    QDBusVariant GetValue(const QString& key);
    bool SetValue(const QString& key, const QDBusVariant& value);
    QVariantMap GetAllSettings();
    void ReloadConfig();

signals:
    void ConfigChanged(const QString& key, const QDBusVariant& value);
    void ThemeChanged(const QString& themeName);

private:
    SettingsBridge* m_bridge;
};

} // namespace tinexus::settings_ui
