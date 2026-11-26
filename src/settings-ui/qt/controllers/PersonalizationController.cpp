#include "PersonalizationController.hpp"
#include <common/DBusNames.hpp>
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <algorithm>
#include <unordered_set>

namespace tinexus::settings_ui {

namespace {
struct AccentDef {
    const char* hex;
    const char* name;
};

const AccentDef ACCENT_TABLE[] = {
    {"#00c3ff", "Electric Cyan"},
    {"#0A84FF", "macOS Blue"},
    {"#5856d6", "Deep Violet"},
    {"#ff2d55", "Hot Pink"},
    {"#ff9500", "Vibrant Orange"},
    {"#30d158", "Mint Green"}
};
constexpr int ACCENT_COUNT = sizeof(ACCENT_TABLE) / sizeof(ACCENT_TABLE[0]);

QString prettifyWallpaperName(const QString& baseName) {
    QString clean = baseName;
    clean.replace(QLatin1Char('-'), QLatin1Char(' '));
    clean.replace(QLatin1Char('_'), QLatin1Char(' '));
    QStringList words = clean.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString& w : words) {
        if (!w.isEmpty()) w[0] = w[0].toUpper();
    }
    return words.join(QLatin1Char(' '));
}

void pushWallpaperOverDbus(const QString& path) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;

    auto sendCall = [&](const QString& svc, const QString& objPath, const QString& iface) {
        QDBusMessage msg = QDBusMessage::createMethodCall(svc, objPath, iface, QStringLiteral("SetWallpaper"));
        msg << path << static_cast<uchar>(0) << false << static_cast<ushort>(500);
        bus.send(msg);
    };
    sendCall(tinexus::common::dbus::qservice::Wallpaper(),
             tinexus::common::dbus::qpath::Wallpaper(),
             tinexus::common::dbus::qinterface::Wallpaper());
    sendCall(tinexus::common::dbus::qservice::legacy::Wallpaper(),
             tinexus::common::dbus::qpath::legacy::Wallpaper(),
             tinexus::common::dbus::qinterface::legacy::Wallpaper());
}
} // anonymous namespace

PersonalizationController::PersonalizationController(QObject* parent)
    : QObject(parent)
{
    scanWallpapers();
}

QString PersonalizationController::accentColor() const {
    int idx = std::clamp(m_accentIndex, 0, ACCENT_COUNT - 1);
    return QString::fromLatin1(ACCENT_TABLE[idx].hex);
}

void PersonalizationController::setAccentIndex(int index) {
    if (index >= 0 && index < ACCENT_COUNT && m_accentIndex != index) {
        m_accentIndex = index;
        emit accentIndexChanged();
        emit accentColorChanged();
        emit toastRequested(QStringLiteral("Accent theme set to %1").arg(QString::fromLatin1(ACCENT_TABLE[m_accentIndex].name)), false);
        emit settingModified(QStringLiteral("accent_index"), index);
        emit settingModified(QStringLiteral("accent_color"), QString::fromLatin1(ACCENT_TABLE[m_accentIndex].hex));
    }
}

void PersonalizationController::setThemeMode(const QString& mode) {
    if (m_themeMode != mode && !mode.isEmpty()) {
        m_themeMode = mode;
        emit themeModeChanged();
        emit toastRequested(QStringLiteral("Theme mode set to %1").arg(mode), false);
        emit settingModified(QStringLiteral("theme_mode"), mode);
        emit settingModified(QStringLiteral("theme"), mode.toLower());
    }
}

void PersonalizationController::setSelectedWallpaperIndex(int index) {
    if (index >= 0 && index < m_wallpapers.size() && m_selectedWallpaperIndex != index) {
        m_selectedWallpaperIndex = index;
        QString path = m_wallpapers[index].toMap().value(QStringLiteral("path")).toString();

        std::string wall_path = tinexus::common::RuntimePaths::get_runtime_dir() + "/current_wallpaper";
        QFile curWall(QString::fromStdString(wall_path));
        if (curWall.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            curWall.write((path + QLatin1Char('\n')).toUtf8());
            curWall.flush();
            curWall.close();
        }

        pushWallpaperOverDbus(path);
        emit selectedWallpaperIndexChanged();
        emit toastRequested(QStringLiteral("Wallpaper applied: %1").arg(m_wallpapers[index].toMap().value(QStringLiteral("name")).toString()), false);
        emit settingModified(QStringLiteral("selected_wallpaper_idx"), index);
        emit settingModified(QStringLiteral("wallpaper_path"), path);
    }
}

void PersonalizationController::scanWallpapers() {
    m_wallpapers.clear();
    std::unordered_set<std::string> seenNames;
    const QString homeDir = QString::fromLocal8Bit(std::getenv("HOME") ? std::getenv("HOME") : "/home/tinexus");

    const QStringList searchDirs = {
        QStringLiteral("/usr/share/backgrounds"),
        homeDir + QStringLiteral("/Pictures"),
        homeDir + QStringLiteral("/Pictures/Wallpapers"),
        QStringLiteral("/workspace/assets/wallpaper"),
    };
    const QStringList filters = {
        QStringLiteral("*.png"), QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"), QStringLiteral("*.webp")
    };
    const char* fallbackColors[] = {"#0f2f2e", "#18223c", "#121b2d", "#4a1d36", "#1f2937"};

    int colorIdx = 0;
    for (const QString& dirPath : searchDirs) {
        QDir d(dirPath);
        if (!d.exists()) continue;
        const QFileInfoList entries = d.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& fi : entries) {
            if (fi.fileName() == QStringLiteral("default.png") && seenNames.count("emerald-matrix.png")) continue;
            const std::string key = fi.fileName().toLower().toStdString();
            if (!seenNames.insert(key).second) continue;

            QVariantMap map;
            map[QStringLiteral("name")] = prettifyWallpaperName(fi.completeBaseName());
            map[QStringLiteral("path")] = fi.absoluteFilePath();
            map[QStringLiteral("previewColor")] = QString::fromLatin1(fallbackColors[colorIdx % 5]);
            ++colorIdx;
            m_wallpapers.append(map);
        }
    }

    if (m_wallpapers.isEmpty()) {
        const struct WallDef { const char* name; const char* fileName; const char* color; } defaults[] = {
            {"Emerald Matrix", "emerald-matrix.png", "#0f2f2e"},
            {"Tinexus Default", "tinexus-default.jpg", "#18223c"},
            {"Tinexus OS Primary", "tinexus-os-primary.jpg", "#121b2d"},
            {"Sunset Gradient", "sunset-gradient.png", "#4a1d36"}
        };
        for (const auto& w : defaults) {
            QVariantMap map;
            map[QStringLiteral("name")] = QString::fromLatin1(w.name);
            map[QStringLiteral("path")] = QStringLiteral("/usr/share/backgrounds/") + QString::fromLatin1(w.fileName);
            map[QStringLiteral("previewColor")] = QString::fromLatin1(w.color);
            m_wallpapers.append(map);
        }
    }

    emit wallpapersChanged();
}

} // namespace tinexus::settings_ui
