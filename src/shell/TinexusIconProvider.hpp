// ============================================================================
// TinexusIconProvider.hpp — High-Resolution SVG Icon Provider for QML
// Ref: 05_UI_UX_GUIDELINES.md §7 (Iconography & Materials)
// ============================================================================
#pragma once

#include <QtQuick/QQuickImageProvider>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QColor>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <QtSvg/QSvgRenderer>
#include <common/logger.hpp>
#include <mutex>

namespace tinexus::shell {

class TinexusIconProvider : public QQuickImageProvider {
public:
    TinexusIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
        // Standard XDG and project asset search paths
        m_searchDirs = {
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/status"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/devices"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/actions"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/ui"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/legacy"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/apps"),
            QStringLiteral("/usr/share/icons/Adwaita/symbolic/categories"),
            QStringLiteral("/usr/share/icons/hicolor/scalable/status"),
            QStringLiteral("/usr/share/icons/hicolor/scalable/apps"),
            QStringLiteral("/usr/share/icons/hicolor/scalable/devices"),
            QStringLiteral("/usr/share/icons/hicolor/scalable/actions"),
            QStringLiteral("/usr/share/tinexus"),
            QStringLiteral("/usr/share/tinexus/logo"),
            QStringLiteral("/usr/share/pixmaps"),
            QStringLiteral("/workspace/assets/logo"),
            QStringLiteral("/workspace/assets"),
            QDir::currentPath() + QStringLiteral("/assets/logo"),
            QDir::currentPath() + QStringLiteral("/assets"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/logo"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../share/tinexus")
        };
    }

    ~TinexusIconProvider() override = default;

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override {
        // Parse ID and query parameters (e.g. "audio-volume-high-symbolic?color=#38BDF8")
        QString cleanName = id;
        QString colorParam;
        int queryIdx = cleanName.indexOf(QLatin1Char('?'));
        if (queryIdx >= 0) {
            QString queryStr = cleanName.mid(queryIdx + 1);
            cleanName = cleanName.left(queryIdx);
            QUrlQuery query(queryStr);
            colorParam = query.queryItemValue(QStringLiteral("color"));
        }

        QSize targetSize = (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0)
                               ? requestedSize
                                : QSize(24, 24);

        if (size) {
            *size = targetSize;
        }

        // Cache lookup
        QString cacheKey = cleanName + QStringLiteral("@") +
                           QString::number(targetSize.width()) + QStringLiteral("x") +
                           QString::number(targetSize.height()) + QStringLiteral(":") + colorParam;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_cache.find(cacheKey);
            if (it != m_cache.end()) {
                return it.value();
            }
        }

        // 1. Direct file resolution via QSvgRenderer
        QString filePath = findIconFile(cleanName);
        QPixmap pixmap;

        if (!filePath.isEmpty()) {
            if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
                QSvgRenderer renderer(filePath);
                if (renderer.isValid()) {
                    QPixmap pm(targetSize);
                    pm.fill(Qt::transparent);
                    QPainter p(&pm);
                    p.setRenderHint(QPainter::Antialiasing);
                    renderer.render(&p);
                    p.end();
                    pixmap = pm;
                }
            } else {
                pixmap.load(filePath);
                if (!pixmap.isNull() && pixmap.size() != targetSize) {
                    pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
        }

        // 2. Resolve via QIcon system theme
        if (pixmap.isNull()) {
            QIcon icon = QIcon::fromTheme(cleanName);
            if (icon.isNull()) {
                if (cleanName.endsWith(QStringLiteral("-symbolic"))) {
                    QString stripped = cleanName.left(cleanName.length() - 9);
                    icon = QIcon::fromTheme(stripped);
                } else {
                    icon = QIcon::fromTheme(cleanName + QStringLiteral("-symbolic"));
                }
            }
            if (!icon.isNull()) {
                pixmap = icon.pixmap(targetSize);
            }
        }

        // 3. If still null, generate geometric vector fallback
        if (pixmap.isNull()) {
            pixmap = createFallbackPixmap(cleanName, targetSize);
        }

        // 4. Apply color modulation / tinting if requested
        if (!colorParam.isEmpty() && !pixmap.isNull()) {
            QColor tint(colorParam);
            if (tint.isValid()) {
                QPixmap tinted(pixmap.size());
                tinted.fill(Qt::transparent);
                QPainter painter(&tinted);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.drawPixmap(0, 0, pixmap);
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(tinted.rect(), tint);
                painter.end();
                pixmap = tinted;
            }
        }

        // Store in cache
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cache.insert(cacheKey, pixmap);
        }

        return pixmap;
    }

private:
    QString findIconFile(const QString& name) const {
        QStringList candidateNames;
        if (name.endsWith(QStringLiteral("-symbolic"))) {
            candidateNames << name + QStringLiteral(".svg")
                           << name + QStringLiteral(".png")
                           << name;
            QString base = name.left(name.length() - 9);
            candidateNames << base + QStringLiteral(".svg")
                           << base + QStringLiteral(".png")
                           << base;
        } else {
            candidateNames << name + QStringLiteral("-symbolic.svg")
                           << name + QStringLiteral(".svg")
                           << name + QStringLiteral(".png")
                           << name;
        }

        for (const auto& dirPath : m_searchDirs) {
            for (const auto& cand : candidateNames) {
                QString fullPath = dirPath + QLatin1Char('/') + cand;
                if (QFileInfo::exists(fullPath)) {
                    return fullPath;
                }
            }
        }
        return QString();
    }

    QPixmap createFallbackPixmap(const QString& name, const QSize& size) const {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        if (name.contains(QStringLiteral("pan-down")) || name.contains(QStringLiteral("arrow-down"))) {
            // Down chevron arrow
            p.setPen(QPen(QColor(255, 255, 255, 180), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            int midX = size.width() / 2;
            int midY = size.height() / 2;
            p.drawLine(midX - 3, midY - 2, midX, midY + 2);
            p.drawLine(midX, midY + 2, midX + 3, midY - 2);
        } else if (name.contains(QStringLiteral("grid"))) {
            // 2x2 or 3x3 dot grid
            p.setBrush(QColor(255, 255, 255, 220));
            p.setPen(Qt::NoPen);
            int step = size.width() / 3;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    p.drawEllipse(3 + c * step, 3 + r * step, 2, 2);
                }
            }
        } else {
            // Subtle circular glyph
            p.setBrush(QColor(56, 189, 248, 160));
            p.setPen(Qt::NoPen);
            int radius = std::min(size.width(), size.height()) / 3;
            p.drawEllipse(size.width() / 2 - radius, size.height() / 2 - radius, radius * 2, radius * 2);
        }
        p.end();
        return pm;
    }

    QStringList m_searchDirs;
    QHash<QString, QPixmap> m_cache;
    std::mutex m_mutex;
};

} // namespace tinexus::shell
