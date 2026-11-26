#pragma once

// ============================================================================
// TinexusAppIconProvider.hpp — High-DPI App Icon Provider for Qt6 Shell Apps
// ============================================================================
// Qt6 UI-only header (never include in pure C++20 non-UI daemons).
// Resolves full-color Freedesktop app icons across Papirus, Adwaita, hicolor,
// pixmaps, and direct paths, rendering SVGs via QSvgRenderer (no dependency on
// optional qsvg imageformat plugins) and falling back through QIcon themes.
// ============================================================================

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtQuick/QQuickImageProvider>
#include <QtSvg/QSvgRenderer>
#include <mutex>

namespace tinexus::common::ui {

class AppIconProvider : public QQuickImageProvider {
public:
    AppIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
        if (QIcon::themeName().isEmpty() || QIcon::themeName() == QStringLiteral("hicolor")) {
            QIcon::setThemeName(QStringLiteral("Papirus"));
        }
        QIcon::setFallbackThemeName(QStringLiteral("Adwaita"));

        m_searchDirs = {
            QStringLiteral("/usr/share/icons/Papirus/64x64/apps"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/apps"),
            QStringLiteral("/usr/share/icons/Papirus/128x128/apps"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/places"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/places"),
            QStringLiteral("/usr/share/icons/Papirus/24x24/places"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/mimetypes"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/mimetypes"),
            QStringLiteral("/usr/share/icons/Papirus/24x24/mimetypes"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/categories"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/categories"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/devices"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/devices"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/actions"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/actions"),
            QStringLiteral("/usr/share/icons/Papirus/24x24/actions"),
            QStringLiteral("/usr/share/icons/Papirus/64x64/status"),
            QStringLiteral("/usr/share/icons/Papirus/48x48/status"),
            QStringLiteral("/usr/share/icons/Papirus-Dark/64x64/apps"),
            QStringLiteral("/usr/share/icons/Papirus-Dark/48x48/apps"),
            QStringLiteral("/usr/share/icons/Papirus-Dark/64x64/places"),
            QStringLiteral("/usr/share/icons/Papirus-Dark/64x64/mimetypes"),
            QStringLiteral("/usr/share/icons/hicolor/128x128/apps"),
            QStringLiteral("/usr/share/icons/hicolor/64x64/apps"),
            QStringLiteral("/usr/share/icons/hicolor/48x48/apps"),
            QStringLiteral("/usr/share/icons/hicolor/scalable/apps"),
            QStringLiteral("/usr/share/icons/hicolor/32x32/apps"),
            QStringLiteral("/usr/share/icons/Adwaita/scalable/apps"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/apps"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/places"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/legacy"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/categories"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/status"),
            QStringLiteral("/usr/share/pixmaps"),
            QStringLiteral("/usr/share/tinexus"),
            QStringLiteral("/usr/share/tinexus/logo"),
            QStringLiteral("/workspace/assets/logo"),
        };
    }

    ~AppIconProvider() override = default;

    static bool canResolveIcon(const QString& iconName) {
        if (iconName.isEmpty()) return false;
        static AppIconProvider s_probe;
        if (!s_probe.findIconFile(iconName).isEmpty()) return true;
        const QString origTheme = QIcon::themeName();
        for (const char* theme : { "Papirus", "Papirus-Dark", "Adwaita", "hicolor" }) {
            QIcon::setThemeName(QString::fromLatin1(theme));
            if (!QIcon::fromTheme(iconName).isNull() ||
                (!iconName.endsWith(QStringLiteral("-symbolic")) &&
                 !QIcon::fromTheme(iconName + QStringLiteral("-symbolic")).isNull())) {
                if (!origTheme.isEmpty()) QIcon::setThemeName(origTheme);
                return true;
            }
        }
        if (!origTheme.isEmpty()) QIcon::setThemeName(origTheme);
        return false;
    }

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override {
        const QString cleanName = id.trimmed();
        const QSize targetSize = (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0)
                                     ? requestedSize
                                     : QSize(64, 64);
        if (cleanName.isEmpty()) return {};

        const QString cacheKey = cleanName + QLatin1Char('@') +
                                 QString::number(targetSize.width()) + QLatin1Char('x') +
                                 QString::number(targetSize.height());
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_cache.find(cacheKey);
            if (it != m_cache.end()) {
                if (size && !it.value().isNull()) *size = it.value().size();
                return it.value();
            }
        }

        QPixmap pixmap;
        const QString filePath = findIconFile(cleanName);
        if (!filePath.isEmpty()) {
            pixmap = loadIconFromFile(filePath, targetSize);
        }

        if (pixmap.isNull()) {
            pixmap = loadViaQIconThemes(cleanName, targetSize);
        }

        if (size && !pixmap.isNull()) {
            *size = pixmap.size();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.insert(cacheKey, pixmap);
        return pixmap;
    }

private:
    QString findIconFile(const QString& name) const {
        if (name.startsWith(QLatin1Char('/')) && QFileInfo::exists(name)) {
            return name;
        }

        QStringList candidates{
            name + QStringLiteral(".svg"),
            name + QStringLiteral(".png"),
            name,
            name + QStringLiteral("-symbolic.svg")
        };

        for (const auto& dir : m_searchDirs) {
            for (const auto& cand : candidates) {
                const QString full = dir + QLatin1Char('/') + cand;
                if (QFileInfo::exists(full)) {
                    return full;
                }
            }
        }
        return {};
    }

    static QPixmap loadIconFromFile(const QString& filePath, const QSize& targetSize) {
        if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
            QSvgRenderer renderer(filePath);
            if (!renderer.isValid()) return {};
            QPixmap pm(targetSize);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            renderer.render(&p);
            p.end();
            return pm;
        }

        QPixmap pm(filePath);
        if (!pm.isNull() && pm.size() != targetSize) {
            pm = pm.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return pm;
    }

    static QPixmap loadViaQIconThemes(const QString& name, const QSize& targetSize) {
        const QString origTheme = QIcon::themeName();
        for (const char* theme : { "Papirus", "Papirus-Dark", "Adwaita", "hicolor" }) {
            QIcon::setThemeName(QString::fromLatin1(theme));
            QIcon icon = QIcon::fromTheme(name);
            if (icon.isNull() && !name.endsWith(QStringLiteral("-symbolic"))) {
                icon = QIcon::fromTheme(name + QStringLiteral("-symbolic"));
            }
            if (!icon.isNull()) {
                QPixmap pm = icon.pixmap(targetSize);
                if (!pm.isNull()) {
                    if (!origTheme.isEmpty()) QIcon::setThemeName(origTheme);
                    return pm;
                }
            }
        }
        if (!origTheme.isEmpty()) QIcon::setThemeName(origTheme);
        return {};
    }

    QStringList m_searchDirs;
    QHash<QString, QPixmap> m_cache;
    std::mutex m_mutex;
};

} // namespace tinexus::common::ui
