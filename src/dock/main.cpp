// ============================================================================
// main.cpp — tinexus-dock (Slice 1: Static Pinned Dock)
// Ref: Architecture Blueprint §1.1, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DockBridge.hpp"
#include "dock/DockModel.hpp"
#include "dock/ToplevelTracker.hpp"
#include "dock/DockMenuPopup.hpp"
#include "dock/DockWindow.hpp"
#include "dock/DnDHandler.hpp"
#include "dock/StacksModel.hpp"
#include "dock/StacksPopup.hpp"
#include "dock/DockIpcClient.hpp"
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <iostream>
#include <csignal>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

int main(int argc, char* argv[]) {
    // ── Ignore SIGPIPE: broken IPC sockets must not kill the dock ────────────
    std::signal(SIGPIPE, SIG_IGN);

    tinexus::log::set_component_name("dock");
    tinexus::log::info("tinexus-dock starting (Slice 2 — Live Window Tracking)...");

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
    // DockBridge: animation engine (fisheye spring math, IPC, icon state, auto-hide)
    tinexus::dock::DockBridge bridge;
    // DockMenuPopup: secondary Layer-Shell overlay surface for context menu (Slice 3)
    tinexus::dock::DockMenuPopup menuPopup;
    bridge.attachMenuPopup(&menuPopup);
    // DockWindow: Layer-Shell host, dynamic exclusive zone & auto-hide tripwire (Slice 4)
    tinexus::dock::DockWindow dockWindow;
    bridge.attachDockWindow(&dockWindow);

    // ── Folder Aggregator Stacks & Popover (Slice 7) ─────────────────────────
    tinexus::dock::StacksModel stacksModel;
    tinexus::dock::StacksPopup stacksPopup(&stacksModel);
    bridge.attachStacksPopup(&stacksPopup);

    // ── Live Wayland Toplevel Tracker (Slice 2) ──────────────────────────────
    tinexus::dock::ToplevelTracker tracker;
    dockModel.setTracker(&tracker);

    // Wire ToplevelTracker signals to DockModel (atomic merge on 'done')
    QObject::connect(&tracker, &tinexus::dock::ToplevelTracker::toplevelAdded,
                     &dockModel, &tinexus::dock::DockModel::onToplevelAddedWithHandle);
    QObject::connect(&tracker, &tinexus::dock::ToplevelTracker::toplevelUpdated,
                     &dockModel, &tinexus::dock::DockModel::onToplevelUpdatedWithHandle);
    QObject::connect(&tracker, &tinexus::dock::ToplevelTracker::toplevelRemoved,
                     &dockModel, &tinexus::dock::DockModel::onToplevelRemovedWithHandle);

    // Auto-Hide / Intellihide: dock auto-hides when a window is maximized or fullscreen
    QObject::connect(&dockModel, &tinexus::dock::DockModel::hasMaximizedWindowsChanged,
                     &dockWindow, &tinexus::dock::DockWindow::onWindowStateChanged);

    // Compositor Loss & Reconnection Logic (Slice 2 recovery)
    auto* reconnectTimer = new QTimer(&app);
    reconnectTimer->setInterval(2000);
    reconnectTimer->setSingleShot(false);

    QObject::connect(&tracker, &tinexus::dock::ToplevelTracker::managerFinished, [&]() {
        tinexus::log::warn("[tinexus-dock] Compositor connection lost — clearing window handles");
        dockModel.clearAllToplevels();
        if (!reconnectTimer->isActive()) {
            reconnectTimer->start();
        }
    });

    QObject::connect(reconnectTimer, &QTimer::timeout, [&]() {
        tinexus::log::info("[tinexus-dock] Attempting to reconnect to foreign toplevel manager...");
        if (tracker.init()) {
            tinexus::log::info("[tinexus-dock] Successfully reconnected to foreign toplevel manager!");
            reconnectTimer->stop();
        }
    });

    // Synchronize DockBridge with live DockModel items and running indicators
    bridge.attachModel(&dockModel);

    // Initialize ToplevelTracker (binds zwlr_foreign_toplevel_manager_v1)
    if (!tracker.init()) {
        tinexus::log::warn("[tinexus-dock] ToplevelTracker::init() reported warning/fallback");
    }

    // ── D-Bus Notification Badges Client (Slice 7) ───────────────────────────
    tinexus::dock::DockIpcClient ipcClient(&dockModel);
    ipcClient.init();

    // ── Wayland Drag-and-Drop Handler (Slice 6) ──────────────────────────────
    tinexus::dock::DnDHandler dndHandler;
    if (!dndHandler.init(tracker.display(), tracker.seat(), &bridge)) {
        tinexus::log::warn("[tinexus-dock] DnDHandler::init() reported warning/fallback");
    }

    // ── QML Engine ───────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"),      &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("dockModel"),   &dockModel);
    engine.rootContext()->setContextProperty(QStringLiteral("tracker"),     &tracker);
    engine.rootContext()->setContextProperty(QStringLiteral("menuPopup"),   &menuPopup);
    engine.rootContext()->setContextProperty(QStringLiteral("dockWindow"),  &dockWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("dndHandler"),  &dndHandler);
    engine.rootContext()->setContextProperty(QStringLiteral("stacksModel"), &stacksModel);
    engine.rootContext()->setContextProperty(QStringLiteral("stacksPopup"), &stacksPopup);
    engine.rootContext()->setContextProperty(QStringLiteral("ipcClient"),   &ipcClient);

    // ── Locate DockBar.qml ───────────────────────────────────────────────────
    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockBar.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockBar.qml"),
        QStringLiteral("src/dock/qml/DockBar.qml"),
        QStringLiteral("/workspace/src/dock/qml/DockBar.qml"),
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

    // Initialize LayerShell window and auto-hide tripwire (Slice 4)
    dockWindow.init(window, &engine, static_cast<int>(bridge.exclusiveZone()));

    window->show();
    tinexus::log::info("[tinexus-dock] Window shown. Initializing context menu overlay...");

    // ── Initialize Overlay Context Menu (Slice 3) ────────────────────────────
    if (!menuPopup.init(&engine)) {
        tinexus::log::warn("[tinexus-dock] DockMenuPopup::init() reported warning or fallback");
    }

    // ── Initialize Stacks Popover Overlay (Slice 7) ──────────────────────────
    if (!stacksPopup.init(&engine)) {
        tinexus::log::warn("[tinexus-dock] StacksPopup::init() reported warning or fallback");
    }

    tinexus::log::info("[tinexus-dock] Entering event loop.");

    return app.exec();
}
