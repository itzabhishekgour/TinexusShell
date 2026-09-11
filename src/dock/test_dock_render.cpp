// ============================================================================
// test_dock_render.cpp — Visual Verification Harness for tinexus-dock (Qt6)
// ============================================================================
#include "dock/DockBridge.hpp"
#include <QtGui/QGuiApplication>
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
    app.setApplicationName(QStringLiteral("test-dock-render"));

    std::cout << "=== Tinexus Dock Visual Test Suite (Qt6) ===" << std::endl;

    tinexus::dock::DockBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockBar.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockBar.qml"),
        QStringLiteral("src/dock/qml/DockBar.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/dock/qml/DockBar.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate DockBar.qml" << std::endl;
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

    auto save_frame = [&](const std::string& filename) -> bool {
        for (int i = 0; i < 10; ++i) {
            app.processEvents();
            usleep(20000);
        }
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            img.save(QString::fromStdString(filename));
            std::cout << "  [SUCCESS] Saved " << filename << " (" << img.width() << "x" << img.height() << ")" << std::endl;
            return true;
        } else {
            std::cerr << "  [FAIL] grabWindow returned null for " << filename << std::endl;
            return false;
        }
    };

    // 1. Idle state
    std::cout << "[Visual Test] Rendering Dock Idle State..." << std::endl;
    bridge.resetHover();
    if (!save_frame("dock_idle.png")) return 1;

    // 2. Hover state over first icon (Terminal)
    std::cout << "[Visual Test] Rendering Dock Hover State (Terminal)..." << std::endl;
    if (!bridge.rawIcons().empty()) {
        bridge.handleHover(bridge.rawIcons()[0].centerX);
        for (int f = 0; f < 30; ++f) {
            bridge.tickAnimations(0.016);
            app.processEvents();
        }
    }
    if (!save_frame("dock_hover_terminal.png")) return 1;

    // 3. Focused state
    std::cout << "[Visual Test] Rendering Dock Focused State..." << std::endl;
    bridge.updateIconState(QStringLiteral("tinexus-terminal"), tinexus::dock::DockIconAppState::RunningFocused);
    bridge.resetHover();
    for (int f = 0; f < 20; ++f) {
        bridge.tickAnimations(0.016);
        app.processEvents();
    }
    if (!save_frame("dock_focused_app.png")) return 1;

    std::cout << "[Visual Test] All dock visual tests passed." << std::endl;
    return 0;
}
