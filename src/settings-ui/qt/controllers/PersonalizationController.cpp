#include "PersonalizationController.hpp"
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <algorithm>

namespace tinexus::settings_ui {

namespace {
struct AccentDef {
    const char* hex;
    const char* name;
};

const AccentDef ACCENT_TABLE[] = {
    {"#00c3ff", "Electric Cyan"},
    {"#007aff", "macOS Blue"},
    {"#5856d6", "Deep Violet"},
    {"#ff2d55", "Hot Pink"},
    {"#ff9500", "Vibrant Orange"},
    {"#34c759", "Mint Green"}
};
constexpr int ACCENT_COUNT = sizeof(ACCENT_TABLE) / sizeof(ACCENT_TABLE[0]);
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
    }
}

void PersonalizationController::setThemeMode(const QString& mode) {
    if (m_themeMode != mode && !mode.isEmpty()) {
        m_themeMode = mode;
        emit themeModeChanged();
        emit toastRequested(QStringLiteral("Theme mode set to %1").arg(mode), false);
        emit settingModified(QStringLiteral("theme_mode"), mode);
    }
}

void PersonalizationController::setSelectedWallpaperIndex(int index) {
    if (index >= 0 && index < m_wallpapers.size() && m_selectedWallpaperIndex != index) {
        m_selectedWallpaperIndex = index;
        QString path = m_wallpapers[index].toMap().value(QStringLiteral("path")).toString();

        // 1. Write to runtime current_wallpaper files if possible
        std::string wall_path = tinexus::common::RuntimePaths::get_runtime_dir() + "/current_wallpaper";
        QFile curWall(QString::fromStdString(wall_path));
        if (curWall.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            curWall.write((path + QStringLiteral("\n")).toUtf8());
            curWall.flush();
            curWall.close();
        }

        emit selectedWallpaperIndexChanged();

        emit toastRequested(QStringLiteral("Wallpaper applied: %1").arg(m_wallpapers[index].toMap().value(QStringLiteral("name")).toString()), false);
        emit settingModified(QStringLiteral("selected_wallpaper_idx"), index);
        emit settingModified(QStringLiteral("wallpaper_path"), path);
    }
}

void PersonalizationController::scanWallpapers() {
    m_wallpapers.clear();
    const struct WallDef {
        const char* name;
        const char* fileName;
        const char* color;
    } walls[] = {
        {"Emerald Matrix",     "emerald-matrix.png",     "#0f2f2e"},
        {"Tinexus Default",    "tinexus-default.jpg",    "#18223c"},
        {"Tinexus OS Primary", "tinexus-os-primary.jpg", "#121b2d"},
        {"Sunset Gradient",    "sunset-gradient.png",    "#4a1d36"}
    };

    for (const auto& w : walls) {
        QVariantMap map;
        map["name"] = QString::fromLatin1(w.name);
        QString path = QStringLiteral("/usr/share/backgrounds/") + QString::fromLatin1(w.fileName);
        if (!QFile::exists(path)) {
            QString alt = QStringLiteral("/home/tinexus/Pictures/") + QString::fromLatin1(w.fileName);
            if (QFile::exists(alt)) {
                path = alt;
            } else if (QFile::exists(QDir::current().absoluteFilePath(QStringLiteral("assets/wallpaper/") + QString::fromLatin1(w.fileName)))) {
                path = QDir::current().absoluteFilePath(QStringLiteral("assets/wallpaper/") + QString::fromLatin1(w.fileName));
            } else {
                path = QStringLiteral("/usr/share/backgrounds/") + QString::fromLatin1(w.fileName);
            }
        }
        map["path"] = path;
        map["previewColor"] = QString::fromLatin1(w.color);
        m_wallpapers.append(map);
    }
    emit wallpapersChanged();
}

} // namespace tinexus::settings_ui
