// ============================================================================
// SettingsAdaptor.cpp — Qt6 D-Bus Adaptor implementation for io.tinexus.Settings
// ============================================================================
#include "settings-ui/SettingsAdaptor.hpp"
#include "SettingsBridge.hpp"

namespace tinexus::settings_ui {

SettingsAdaptor::SettingsAdaptor(SettingsBridge* parent)
    : QDBusAbstractAdaptor(parent), m_bridge(parent)
{
    setAutoRelaySignals(true);
}

QString SettingsAdaptor::currentTheme() const {
    return m_bridge ? m_bridge->themeMode() : QStringLiteral("Dark");
}

void SettingsAdaptor::setCurrentTheme(const QString& theme) {
    if (m_bridge) {
        m_bridge->setThemeMode(theme);
        emit ThemeChanged(theme);
        emit ConfigChanged(QStringLiteral("CurrentTheme"), QDBusVariant(theme));
    }
}

QString SettingsAdaptor::language() const {
    return QStringLiteral("en_US");
}

void SettingsAdaptor::setLanguage(const QString& lang) {
    emit ConfigChanged(QStringLiteral("Language"), QDBusVariant(lang));
}

QDBusVariant SettingsAdaptor::GetValue(const QString& key) {
    if (!m_bridge) return QDBusVariant(QVariant());

    QString lower = key.toLower();
    if (lower == QStringLiteral("theme") || lower == QStringLiteral("currenttheme")) {
        return QDBusVariant(m_bridge->themeMode());
    } else if (lower == QStringLiteral("language")) {
        return QDBusVariant(QStringLiteral("en_US"));
    } else if (lower == QStringLiteral("brightness")) {
        return QDBusVariant(m_bridge->brightness());
    } else if (lower == QStringLiteral("volume")) {
        return QDBusVariant(m_bridge->volume());
    } else if (lower == QStringLiteral("muted")) {
        return QDBusVariant(m_bridge->muted());
    } else if (lower == QStringLiteral("nightlight")) {
        return QDBusVariant(m_bridge->nightLight());
    } else if (lower == QStringLiteral("vrrenabled")) {
        return QDBusVariant(m_bridge->vrrEnabled());
    } else if (lower == QStringLiteral("accentindex")) {
        return QDBusVariant(m_bridge->accentIndex());
    } else if (lower == QStringLiteral("accentcolor")) {
        return QDBusVariant(m_bridge->accentColor());
    } else if (lower == QStringLiteral("wallpaperindex") || lower == QStringLiteral("selectedwallpaperindex")) {
        return QDBusVariant(m_bridge->selectedWallpaperIndex());
    } else if (lower == QStringLiteral("wifi") || lower == QStringLiteral("wifienabled")) {
        return QDBusVariant(m_bridge->wifiEnabled());
    } else if (lower == QStringLiteral("screentimeout") || lower == QStringLiteral("screentimeoutmin")) {
        return QDBusVariant(m_bridge->screenTimeoutMin());
    } else if (lower == QStringLiteral("sleepafter") || lower == QStringLiteral("sleepaftermin")) {
        return QDBusVariant(m_bridge->sleepAfterMin());
    } else if (lower == QStringLiteral("powerprofile") || lower == QStringLiteral("powerprofileindex")) {
        return QDBusVariant(m_bridge->powerProfileIndex());
    } else if (lower == QStringLiteral("lockonsleep")) {
        return QDBusVariant(m_bridge->lockOnSleep());
    } else if (lower == QStringLiteral("clipboardhistorysize")) {
        return QDBusVariant(m_bridge->clipboardHistorySize());
    }

    return QDBusVariant(QVariant());
}

bool SettingsAdaptor::SetValue(const QString& key, const QDBusVariant& value) {
    if (!m_bridge) return false;

    QString lower = key.toLower();
    QVariant val = value.variant();

    if (lower == QStringLiteral("theme") || lower == QStringLiteral("currenttheme")) {
        QString theme = val.toString();
        m_bridge->setThemeMode(theme);
        emit ThemeChanged(theme);
    } else if (lower == QStringLiteral("brightness")) {
        m_bridge->setBrightness(val.toInt());
    } else if (lower == QStringLiteral("volume")) {
        m_bridge->setVolume(val.toInt());
    } else if (lower == QStringLiteral("muted")) {
        m_bridge->setMuted(val.toBool());
    } else if (lower == QStringLiteral("nightlight")) {
        m_bridge->setNightLight(val.toBool());
    } else if (lower == QStringLiteral("vrrenabled")) {
        m_bridge->setVrrEnabled(val.toBool());
    } else if (lower == QStringLiteral("accentindex")) {
        m_bridge->setAccentIndex(val.toInt());
    } else if (lower == QStringLiteral("wallpaperindex") || lower == QStringLiteral("selectedwallpaperindex")) {
        m_bridge->setSelectedWallpaperIndex(val.toInt());
    } else if (lower == QStringLiteral("wifi") || lower == QStringLiteral("wifienabled")) {
        m_bridge->setWifiEnabled(val.toBool());
    } else if (lower == QStringLiteral("screentimeout") || lower == QStringLiteral("screentimeoutmin")) {
        m_bridge->setScreenTimeoutMin(val.toInt());
    } else if (lower == QStringLiteral("sleepafter") || lower == QStringLiteral("sleepaftermin")) {
        m_bridge->setSleepAfterMin(val.toInt());
    } else if (lower == QStringLiteral("powerprofile") || lower == QStringLiteral("powerprofileindex")) {
        m_bridge->setPowerProfileIndex(val.toInt());
    } else if (lower == QStringLiteral("lockonsleep")) {
        m_bridge->setLockOnSleep(val.toBool());
    } else if (lower == QStringLiteral("clipboardhistorysize")) {
        m_bridge->setClipboardHistorySize(val.toInt());
    }

    emit ConfigChanged(key, value);
    return true;
}

QVariantMap SettingsAdaptor::GetAllSettings() {
    QVariantMap map;
    if (!m_bridge) return map;

    map.insert(QStringLiteral("CurrentTheme"), m_bridge->themeMode());
    map.insert(QStringLiteral("Language"), QStringLiteral("en_US"));
    map.insert(QStringLiteral("Brightness"), m_bridge->brightness());
    map.insert(QStringLiteral("Volume"), m_bridge->volume());
    map.insert(QStringLiteral("Muted"), m_bridge->muted());
    map.insert(QStringLiteral("NightLight"), m_bridge->nightLight());
    map.insert(QStringLiteral("VrrEnabled"), m_bridge->vrrEnabled());
    map.insert(QStringLiteral("AccentIndex"), m_bridge->accentIndex());
    map.insert(QStringLiteral("AccentColor"), m_bridge->accentColor());
    map.insert(QStringLiteral("SelectedWallpaperIndex"), m_bridge->selectedWallpaperIndex());
    map.insert(QStringLiteral("WifiEnabled"), m_bridge->wifiEnabled());
    map.insert(QStringLiteral("ScreenTimeoutMin"), m_bridge->screenTimeoutMin());
    map.insert(QStringLiteral("SleepAfterMin"), m_bridge->sleepAfterMin());
    map.insert(QStringLiteral("PowerProfileIndex"), m_bridge->powerProfileIndex());
    map.insert(QStringLiteral("LockOnSleep"), m_bridge->lockOnSleep());
    map.insert(QStringLiteral("ClipboardHistorySize"), m_bridge->clipboardHistorySize());

    return map;
}

void SettingsAdaptor::ReloadConfig() {
    if (m_bridge) {
        m_bridge->loadConfig();
    }
}

} // namespace tinexus::settings_ui

#include "moc_SettingsAdaptor.cpp"
