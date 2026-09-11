// ============================================================================
// main.cpp — tinexus-dock (Qt6 / Layer-shell)
// ============================================================================
#include "dock/DockBridge.hpp"
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("dock");
    tinexus::log::info("tinexus-dock starting (Qt6)...");

    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-dock"));
    app.setDesktopFileName(QStringLiteral("io.tinexus.shell.Dock"));

    tinexus::dock::DockBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockBar.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockBar.qml"),
        QStringLiteral("src/dock/qml/DockBar.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/dock/qml/DockBar.qml"),
        QStringLiteral("/usr/share/tinexus/dock/qml/DockBar.qml")
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
        std::cerr << "FAIL: Failed to load root QML object for dock" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
        auto* lsWin = LayerShellQt::Window::get(window);
        if (lsWin) {
            lsWin->setLayer(LayerShellQt::Window::LayerTop);
            lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(LayerShellQt::Window::AnchorBottom |
                                                                    LayerShellQt::Window::AnchorLeft |
                                                                    LayerShellQt::Window::AnchorRight));
            lsWin->setExclusiveZone(96);
            std::cout << "[tinexus-dock] LayerShellQt configured: Layer=Top, ExclusiveZone=96" << std::endl;
        }
#else
        std::cout << "[tinexus-dock] LayerShellQt not linked — running in fallback QWindow mode (exclusive zone not registered with compositor)" << std::endl;
#endif
        window->show();
    }

    return app.exec();
}
