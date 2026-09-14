// ============================================================================
// test_shell_render.cpp — Visual Verification Harness for tinexus-shell (Qt6)
// ============================================================================
#include "ShellBridge.hpp"
#include "TinexusIconProvider.hpp"
#include <QtGui/QGuiApplication>
#include <QtGui/QFontDatabase>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <iostream>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-shell-render"));

    // ── Enforce "Inter" System Typography ──────────────────────────────────────
    QStringList fontPaths = {
        QStringLiteral("/usr/share/fonts/truetype/inter/Inter-Regular.ttf"),
        QStringLiteral("/usr/share/fonts/truetype/inter/Inter-Bold.ttf"),
        QStringLiteral("/workspace/assets/fonts/Inter-Regular.ttf"),
        QStringLiteral("/workspace/assets/fonts/Inter-Bold.ttf")
    };
    for (const auto& fp : fontPaths) {
        if (QFileInfo::exists(fp)) {
            QFontDatabase::addApplicationFont(fp);
        }
    }
    QFont defaultFont(QStringLiteral("Inter"));
    defaultFont.setPixelSize(12);
    app.setFont(defaultFont);

    std::cout << "[Visual Test] Rendering Desktop Top Bar and Flyouts (Qt6)..." << std::endl;

    tinexus::shell::ShellBridge bridge;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new tinexus::shell::TinexusIconProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DesktopShellWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/shell/qml/DesktopShellWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate DesktopShellWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        std::cerr << "FAIL: Root object is not a QQuickWindow" << std::endl;
        return 1;
    }

    window->show();

    auto save_frame = [&](int targetW, int targetH, const std::string& filename) -> bool {
        window->resize(targetW, targetH);
        for (int i = 0; i < 8; ++i) {
            app.processEvents();
            usleep(20000);
        }
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            if (img.width() != targetW || img.height() != targetH) {
                img = img.copy(0, 0, targetW, targetH);
            }
            img.save(QString::fromStdString(filename));
            std::cout << "[Visual Test] Successfully saved " << filename
                      << " (" << targetW << "x" << targetH << ")" << std::endl;
            return true;
        } else {
            std::cerr << "FAIL: Failed to save " << filename << std::endl;
            return false;
        }
    };

    // ── 1. Idle Top Bar (1920 x 46) ──────────────────────────────────────────
    bridge.closeAllFlyouts();
    bridge.setNotchExpanded(false);
    if (!save_frame(1920, 46, "desktop_topbar_idle.png")) return 1;

    // ── 2. Dynamic Island / Aura Notch Expanded (1920 x 84) ──────────────────
    bridge.closeAllFlyouts();
    bridge.setNotchExpanded(true);
    if (!save_frame(1920, 84, "desktop_notch_media_open.png")) return 1;
    bridge.setNotchExpanded(false);

    // ── 2. Logo Menu Open (1920 x 290) ───────────────────────────────────────
    bridge.setLogoMenuOpen(true);
    if (!save_frame(1920, 290, "desktop_logo_menu_open.png")) return 1;

    // ── 3. Calendar Flyout Open (1920 x 330) ─────────────────────────────────
    bridge.setCalendarOpen(true);
    if (!save_frame(1920, 330, "desktop_calendar_open.png")) return 1;

    // ── 4. Notification Banners Stack (1920 x 350) ───────────────────────────
    bridge.setNotificationsOpen(true);
    if (!save_frame(1920, 350, "desktop_notifs_open.png")) return 1;

    // ── 5. Volume Slider Flyout (1920 x 220) ─────────────────────────────────
    bridge.setVolumeFlyoutOpen(true);
    if (!save_frame(1920, 220, "desktop_volume_open.png")) return 1;

    // ── 6. Brightness Slider Flyout (1920 x 220) ─────────────────────────────
    bridge.setBrightnessFlyoutOpen(true);
    if (!save_frame(1920, 220, "desktop_brightness_open.png")) return 1;

    std::cout << "[Visual Test] All shell visual tests passed." << std::endl;
    return 0;
}
