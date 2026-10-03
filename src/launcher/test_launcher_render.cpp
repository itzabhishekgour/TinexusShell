// ============================================================================
// test_launcher_render.cpp — Visual Verification Harness for tinexus-launcher (Qt6)
// ============================================================================
#include "LauncherBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <iostream>
#include <vector>
#include <unistd.h>

static std::vector<tinexus::launcher::LauncherItem> get_mock_apps() {
    return {
        {"Tinexus Terminal", "tinexus-terminal", "Default Wayland GPU Terminal", "utilities-terminal", "App", true},
        {"Tinexus Files", "tinexus-files", "Lightweight Miller Column File Manager", "system-file-manager", "App", false},
        {"Tinexus Settings", "tinexus-settings-ui", "System Configuration & Control Center", "preferences-desktop", "App", false},
        {"Activity Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", "utilities-system-monitor", "App", false},
        {"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", "system-software-install", "App", false},
    };
}

int main(int argc, char* argv[]) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-launcher-render"));

    std::cout << "==================================================" << std::endl;
    std::cout << " TxUI -> Qt6 Launcher Visual Verification Harness " << std::endl;
    std::cout << "==================================================" << std::endl;

    tinexus::launcher::LauncherBridge bridge;
    bridge.setAllApps(get_mock_apps());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/LauncherWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("/workspace/src/launcher/qml/LauncherWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate LauncherWindow.qml" << std::endl;
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

    auto save_frame = [&](const std::string& name, const std::string& filename) -> bool {
        std::cout << "[Visual Test] Rendering Launcher: " << name << " -> " << filename << std::endl;
        window->resize(900, 700);

        for (int i = 0; i < 8; ++i) {
            app.processEvents();
            usleep(20000);
        }

        QImage img = window->grabWindow();
        if (!img.isNull()) {
            QImage canvas(900, 700, QImage::Format_ARGB32_Premultiplied);
            canvas.fill(QColor(8, 9, 14, 255));

            QPainter p(&canvas);
            // Center the launcher window on canvas
            int x = (900 - img.width()) / 2;
            int y = (700 - img.height()) / 2;
            p.drawImage(x, y, img);
            p.end();

            if (canvas.save(QString::fromStdString(filename))) {
                std::cout << "  [SUCCESS] Saved " << filename << " (" << canvas.width() << "x" << canvas.height() << ")" << std::endl;
                return true;
            } else {
                std::cerr << "  [FAILURE] Could not save " << filename << std::endl;
                return false;
            }
        } else {
            std::cerr << "  [FAILURE] grabWindow returned null for " << filename << std::endl;
            return false;
        }
    };

    // 1. Empty State
    bridge.setQuery(QStringLiteral(""));
    bridge.setSelectedIndex(0);
    if (!save_frame("Launcher Empty State", "launcher_empty_state.png")) return 1;

    // 2. Search Results
    bridge.setQuery(QStringLiteral("sett"));
    bridge.selectNext();
    if (!save_frame("Launcher Search Results", "launcher_search_results.png")) return 1;

    // 3. Calculator
    bridge.setQuery(QStringLiteral("42 * 8"));
    bridge.setSelectedIndex(0);
    if (!save_frame("Launcher Calculator", "launcher_calculator.png")) return 1;

    // 4. Store Fallback
    bridge.setQuery(QStringLiteral("chrome"));
    bridge.setSelectedIndex(0);
    if (!save_frame("Launcher Store Fallback", "launcher_store_search.png")) return 1;

    std::cout << "[Visual Test] All launcher visual tests passed." << std::endl;
    return 0;
}
