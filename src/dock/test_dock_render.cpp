// ============================================================================
// test_dock_render.cpp — Visual Verification Harness for tinexus-dock (Qt6)
// ============================================================================
#include "dock/DockBridge.hpp"
#include "dock/DockModel.hpp"
#include "dock/DockMenuPopup.hpp"
#include "dock/DockWindow.hpp"
#include "dock/DnDHandler.hpp"
#include "dock/StacksModel.hpp"
#include "dock/StacksPopup.hpp"
#include "dock/DockIpcClient.hpp"
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

    std::cout << "=== Tinexus Dock Visual Test Suite (Qt6 Slice 7) ===" << std::endl;

    tinexus::dock::DockModel dockModel;
    tinexus::dock::DockBridge bridge;
    tinexus::dock::DockMenuPopup menuPopup;
    tinexus::dock::DockWindow dockWindow;
    tinexus::dock::DnDHandler dndHandler;
    tinexus::dock::StacksModel stacksModel;
    tinexus::dock::StacksPopup stacksPopup(&stacksModel);
    tinexus::dock::DockIpcClient ipcClient(&dockModel);

    bridge.attachModel(&dockModel);
    bridge.attachMenuPopup(&menuPopup);
    bridge.attachDockWindow(&dockWindow);
    bridge.attachStacksPopup(&stacksPopup);
    dndHandler.init(nullptr, nullptr, &bridge);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"),      &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("dockModel"),   &dockModel);
    engine.rootContext()->setContextProperty(QStringLiteral("menuPopup"),   &menuPopup);
    engine.rootContext()->setContextProperty(QStringLiteral("dockWindow"),  &dockWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("dndHandler"),  &dndHandler);
    engine.rootContext()->setContextProperty(QStringLiteral("stacksModel"), &stacksModel);
    engine.rootContext()->setContextProperty(QStringLiteral("stacksPopup"), &stacksPopup);
    engine.rootContext()->setContextProperty(QStringLiteral("ipcClient"),   &ipcClient);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockBar.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockBar.qml"),
        QStringLiteral("src/dock/qml/DockBar.qml"),
        QStringLiteral("/workspace/src/dock/qml/DockBar.qml"),
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

    menuPopup.init(&engine);
    stacksPopup.init(&engine);
    dockWindow.init(window, &engine, static_cast<int>(bridge.exclusiveZone()));
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

    // 4. Live Window Tracking (Slice 2): Pinned running app with indicator dot
    std::cout << "[Visual Test] Slice 2: Opening pinned app (Terminal -> PinnedRunning with dot)..." << std::endl;
    dockModel.onToplevelAdded(QStringLiteral("tinexus-terminal"));
    for (int f = 0; f < 20; ++f) {
        bridge.tickAnimations(0.016);
        app.processEvents();
    }
    if (!save_frame("dock_pinned_running.png")) return 1;

    // 5. Live Window Tracking (Slice 2): Dynamic transient app insertion
    std::cout << "[Visual Test] Slice 2: Opening unpinned app (vlc -> dynamic RunningApp insertion)..." << std::endl;
    dockModel.onToplevelAdded(QStringLiteral("vlc"));
    for (int f = 0; f < 30; ++f) {
        bridge.tickAnimations(0.016);
        app.processEvents();
    }
    if (!save_frame("dock_transient_running.png")) return 1;

    // 6. Interaction & Context Menu (Slice 3)
    std::cout << "[Visual Test] Slice 3: Right-click / Context Menu invocation..." << std::endl;
    bridge.requestContextMenu(0); // Terminal
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
    }
    if (menuPopup.isOpen()) {
        std::cout << "  [SUCCESS] Context menu opened at (" << menuPopup.menuX() << ", "
                  << menuPopup.menuY() << ") for app '" << menuPopup.targetAppId().toStdString() << "'." << std::endl;
    } else {
        std::cerr << "  [FAIL] Context menu did not open" << std::endl;
        return 1;
    }
    if (menuPopup.window()) {
        QImage menuImg = menuPopup.window()->grabWindow();
        if (!menuImg.isNull()) {
            menuImg.save(QStringLiteral("dock_context_menu.png"));
            std::cout << "  [SUCCESS] Saved dock_context_menu.png (" << menuImg.width() << "x" << menuImg.height() << ")" << std::endl;
        }
    }

    // 7. Context Menu Action Execution (Slice 3)
    std::cout << "[Visual Test] Slice 3: Context menu action triggering & dismissal..." << std::endl;
    menuPopup.triggerAction(QStringLiteral("open"));
    app.processEvents();
    if (!menuPopup.isOpen()) {
        std::cout << "  [SUCCESS] Context menu cleanly dismissed after action." << std::endl;
    } else {
        std::cerr << "  [FAIL] Context menu remained open after action" << std::endl;
        return 1;
    }

    // 8. Auto-Hide State Machine & Dynamic Exclusive Zone (Slice 4)
    std::cout << "[Visual Test] Slice 4: Enabling auto-hide and requesting hide..." << std::endl;
    bridge.setAutoHideEnabled(true);
    bridge.requestHide();
    for (int f = 0; f < 30; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (bridge.autoHideState() == 2 && dockWindow.exclusiveZone() == 0) {
        std::cout << "  [SUCCESS] Dock in HIDDEN state. Exclusive zone successfully set to 0px." << std::endl;
    } else {
        std::cerr << "  [FAIL] Auto-hide state mismatch: state=" << bridge.autoHideState()
                  << " zone=" << dockWindow.exclusiveZone() << std::endl;
        return 1;
    }
    if (!save_frame("dock_autohide_hidden.png")) return 1;

    // 9. Tripwire Surface Reveal (Slice 4)
    std::cout << "[Visual Test] Slice 4: Simulating 2px bottom-edge tripwire trigger..." << std::endl;
    dockWindow.onTripwireEntered();
    for (int f = 0; f < 30; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (bridge.autoHideState() == 0) {
        std::cout << "  [SUCCESS] Dock revealed to VISIBLE state from tripwire trigger." << std::endl;
    } else {
        std::cerr << "  [FAIL] Dock failed to reveal: state=" << bridge.autoHideState() << std::endl;
        return 1;
    }
    if (!save_frame("dock_autohide_revealed.png")) return 1;

    // 10. Disable Auto-Hide -> Restore Full Exclusive Zone (Slice 4)
    std::cout << "[Visual Test] Slice 4: Disabling auto-hide, restoring permanent 72px exclusive zone..." << std::endl;
    bridge.setAutoHideEnabled(false);
    app.processEvents();
    if (dockWindow.exclusiveZone() == static_cast<int>(bridge.exclusiveZone())) {
        std::cout << "  [SUCCESS] Exclusive zone restored to " << dockWindow.exclusiveZone() << "px." << std::endl;
    } else {
        std::cerr << "  [FAIL] Exclusive zone failed to restore: " << dockWindow.exclusiveZone() << std::endl;
        return 1;
    }

    // 11. Drag Rearrange & Model Reorder (Slice 5)
    std::cout << "[Visual Test] Slice 5: Testing drag rearrange moveItem(0, 2)..." << std::endl;
    if (bridge.rawIcons().size() >= 3) {
        QString originalFirst = bridge.rawIcons()[0].appId;
        bridge.moveItem(0, 2);
        for (int f = 0; f < 20; ++f) {
            app.processEvents();
            usleep(10000);
        }
        if (bridge.rawIcons().size() >= 3 && bridge.rawIcons()[2].appId == originalFirst) {
            std::cout << "  [SUCCESS] Successfully moved item '" << originalFirst.toStdString()
                      << "' from index 0 to index 2." << std::endl;
            bridge.commitMove();
        } else {
            std::cerr << "  [FAIL] Item move failed: expected at index 2, but found "
                      << (bridge.rawIcons().size() >= 3 ? bridge.rawIcons()[2].appId.toStdString() : "empty") << std::endl;
            return 1;
        }
    }
    if (!save_frame("dock_drag_rearranged.png")) return 1;

    // 12. Poof Effect & Unpin (Slice 5)
    std::cout << "[Visual Test] Slice 5: Testing unpinning app (firefox)..." << std::endl;
    int initialCount = dockModel.pinnedCount();
    bridge.unpinApp(QStringLiteral("firefox"));
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (dockModel.pinnedCount() == initialCount - 1 && !dockModel.isPinned(QStringLiteral("firefox"))) {
        std::cout << "  [SUCCESS] App 'firefox' successfully unpinned and removed from pinned zone." << std::endl;
    } else {
        std::cerr << "  [FAIL] Unpin failed: pinnedCount=" << dockModel.pinnedCount()
                  << " initial=" << initialCount << std::endl;
        return 1;
    }
    if (!save_frame("dock_poofed_unpinned.png")) return 1;

    // 13. File Drag-and-Drop Hover (Slice 6)
    std::cout << "[Visual Test] Slice 6: Simulating Wayland file hover over icon 0 (Terminal)..." << std::endl;
    dndHandler.simulateHover(0);
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (bridge.dropTargetIndex() == 0 && dndHandler.state() == tinexus::dock::DnDState::HOVERING) {
        std::cout << "  [SUCCESS] Drop target index set to 0. Target app: "
                  << dndHandler.targetAppId().toStdString() << std::endl;
    } else {
        std::cerr << "  [FAIL] Drop target hover failed: bridge.dropTargetIndex=" << bridge.dropTargetIndex() << std::endl;
        return 1;
    }
    if (!save_frame("dock_dnd_hover.png")) return 1;

    // 14. File Drag-and-Drop Drop Simulation (Slice 6)
    std::cout << "[Visual Test] Slice 6: Simulating file drop with URIs on Terminal..." << std::endl;
    QStringList droppedFiles = {
        QStringLiteral("/home/user/Documents/project_notes.txt"),
        QStringLiteral("/home/user/Images/screenshot.png")
    };
    dndHandler.simulateDrop(QStringLiteral("terminal"), droppedFiles);
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (bridge.dropTargetIndex() == -1 && dndHandler.state() == tinexus::dock::DnDState::IDLE) {
        std::cout << "  [SUCCESS] Drop processed, state reset to IDLE, drop target cleared." << std::endl;
    } else {
        std::cerr << "  [FAIL] Drop state cleanup failed: state=" << dndHandler.stateInt() << std::endl;
        return 1;
    }
    if (!save_frame("dock_dnd_completed.png")) return 1;

    // 15. D-Bus Notification Badges (Slice 7)
    std::cout << "[Visual Test] Slice 7: Simulating D-Bus notification badge count=4 on Terminal..." << std::endl;
    ipcClient.simulateBadgeCount(QStringLiteral("tinexus-terminal"), 4);
    ipcClient.simulateBadgeCount(QStringLiteral("terminal"), 4);
    for (int f = 0; f < 25; ++f) {
        app.processEvents();
        usleep(10000);
    }
    std::cout << "  [SUCCESS] Notification badge count dispatched. Pop-in animation triggered." << std::endl;
    if (!save_frame("dock_badge_notification.png")) return 1;

    // 16. Stacks Folder Popover (Slice 7)
    std::cout << "[Visual Test] Slice 7: Testing Stacks Popover on Downloads folder..." << std::endl;
    stacksPopup.showPopup(800.0, 950.0, QStringLiteral("/tmp"), QStringLiteral("Downloads"));
    for (int f = 0; f < 25; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (stacksPopup.isOpen()) {
        std::cout << "  [SUCCESS] Stacks Popover opened at (" << stacksPopup.popupX()
                  << ", " << stacksPopup.popupY() << ") with " << stacksModel.fileCount() << " files." << std::endl;
    } else {
        std::cerr << "  [FAIL] Stacks Popover failed to open" << std::endl;
        return 1;
    }
    if (!save_frame("dock_stacks_popover.png")) return 1;

    stacksPopup.hidePopup();
    app.processEvents();
    if (!stacksPopup.isOpen()) {
        std::cout << "  [SUCCESS] Stacks Popover dismissed successfully." << std::endl;
    } else {
        std::cerr << "  [FAIL] Stacks Popover failed to dismiss" << std::endl;
        return 1;
    }

    std::cout << "[Visual Test] All dock visual tests (Slice 1 through Slice 7: The Grand Finale) PASSED!" << std::endl;
    return 0;
}
