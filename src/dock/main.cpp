// ============================================================================
// main.cpp — tinexus-dock (Slice 1: Static Pinned Dock)
// Ref: Architecture Blueprint §1.1, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DockBridge.hpp"
#include "dock/DockModel.hpp"
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>
#include <csignal>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

int main(int argc, char* argv[]) {
    // ── Ignore SIGPIPE: broken IPC sockets must not kill the dock ────────────
    std::signal(SIGPIPE, SIG_IGN);

    tinexus::log::set_component_name("dock");
    tinexus::log::info("tinexus-dock starting (Slice 1 — Static Pinned Dock)...");

    // ── Force Wayland platform with LayerShell integration ───────────────────
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-dock"));
    app.setDesktopFileName(QStringLiteral("io.tinexus.shell.Dock"));

    // ── Data Models & Bridge ─────────────────────────────────────────────────
    // DockModel: the QAbstractListModel — pinned items, separator, trash
    tinexus::dock::DockModel  dockModel;
    // DockBridge: animation engine (fisheye spring math, IPC, icon state)
    tinexus::dock::DockBridge bridge;

    // ── QML Engine ───────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"),    &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("dockModel"), &dockModel);

    // ── Locate DockBar.qml ───────────────────────────────────────────────────
    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockBar.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockBar.qml"),
        QStringLiteral("src/dock/qml/DockBar.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/dock/qml/DockBar.qml"),
        QStringLiteral("/usr/share/tinexus/dock/qml/DockBar.qml"),
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) { qmlPath = cand; break; }
    }
    if (qmlPath.isEmpty()) {
        std::cerr << "[tinexus-dock] FAIL: Could not locate DockBar.qml\n";
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "[tinexus-dock] FAIL: Failed to load root QML object\n";
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        std::cerr << "[tinexus-dock] FAIL: Root QML object is not a QQuickWindow\n";
        return 1;
    }

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsWin = LayerShellQt::Window::get(window);
    if (lsWin) {
        // ── Decision A: LAYER_TOP — dock sits above maximized/tiled windows ──
        lsWin->setLayer(LayerShellQt::Window::LayerTop);

        // ── Bottom edge anchoring: full-width, exclusive zone ────────────────
        lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft   |
            LayerShellQt::Window::AnchorRight));

        // Exclusive zone = pillHeight + botMargin = (48 + 10*2) + 8 = 76px
        // This tells the compositor no window may extend under the dock.
        const int exclusiveZone = static_cast<int>(bridge.exclusiveZone());
        lsWin->setExclusiveZone(exclusiveZone);

        // ── Decision C: tinexus_blur_v1 via "dock" scope ─────────────────────
        // BlurManager in tinexus-comp has "dock" in its allowed namespaces.
        lsWin->setScope(QStringLiteral("dock"));

        // Keyboard: not interactive — clicks pass through to apps
        lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

        tinexus::log::info(
            "[tinexus-dock] LayerShellQt: Layer=TOP, Anchor=BOTTOM|LEFT|RIGHT, "
            "ExclusiveZone={}, Scope=dock, Keyboard=None",
            exclusiveZone);
    } else {
        tinexus::log::warn("[tinexus-dock] LayerShellQt::Window::get returned nullptr — "
                           "running in fallback QWindow mode (exclusive zone not registered)");
    }
#else
    tinexus::log::warn("[tinexus-dock] LayerShellQt not linked — fallback QWindow mode");
#endif

    window->show();
    tinexus::log::info("[tinexus-dock] Window shown. Entering event loop.");

    return app.exec();
}
