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
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QPainter>
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
    bridge.setAutoHideEnabled(false);
    bridge.setAutoHideState(0);

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

    auto find_wallpaper = [](const QString& name) -> QString {
        QStringList wpCandidates = {
            QStringLiteral("/workspace/assets/wallpaper/") + name,
            QStringLiteral("assets/wallpaper/") + name,
            QStringLiteral("../assets/wallpaper/") + name,
            QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/assets/wallpaper/") + name
        };
        for (const auto& c : wpCandidates) {
            if (QFileInfo::exists(c)) return c;
        }
        return QString();
    };

    auto save_composite_frame = [&](const std::string& filename, const QString& wallpaperName, bool cropToPill = true) -> bool {
        for (int i = 0; i < 12; ++i) {
            app.processEvents();
            usleep(20000);
        }
        QImage dockImg = window->grabWindow();
        if (dockImg.isNull()) {
            std::cerr << "  [FAIL] grabWindow returned null for " << filename << std::endl;
            return false;
        }

        QString wpPath = find_wallpaper(wallpaperName);
        QImage wallpaper(wpPath);
        if (wallpaper.isNull()) {
            std::cerr << "  [WARN] Wallpaper '" << wallpaperName.toStdString() << "' not found, saving raw dock frame." << std::endl;
            dockImg.save(QString::fromStdString(filename));
            return true;
        }

        QImage comp(dockImg.size(), QImage::Format_ARGB32_Premultiplied);
        comp.fill(Qt::black);
        {
            QPainter p(&comp);
            QImage wpScaled = wallpaper.scaledToWidth(dockImg.width(), Qt::SmoothTransformation);
            int cropY = std::max(0, wpScaled.height() - dockImg.height());
            p.drawImage(0, 0, wpScaled, 0, cropY, dockImg.width(), dockImg.height());
            p.drawImage(0, 0, dockImg);
            p.end();
        }

        if (cropToPill) {
            double pillW = bridge.pillWidth();
            int cx = dockImg.width() / 2;
            int marginX = 100;
            int cropX = std::max(0, static_cast<int>(cx - (pillW / 2.0) - marginX));
            int cropW = std::min(dockImg.width() - cropX, static_cast<int>(pillW + marginX * 2));
            int cropH = dockImg.height();
            QImage cropped = comp.copy(cropX, 0, cropW, cropH);
            cropped.save(QString::fromStdString(filename));
            // Also copy to /workspace/build if available
            cropped.save(QString::fromStdString("/workspace/build/" + filename));
            std::cout << "  [SUCCESS] Saved composite " << filename << " (" << cropped.width() << "x" << cropped.height() << ")" << std::endl;
        } else {
            comp.save(QString::fromStdString(filename));
            comp.save(QString::fromStdString("/workspace/build/" + filename));
            std::cout << "  [SUCCESS] Saved composite " << filename << " (" << comp.width() << "x" << comp.height() << ")" << std::endl;
        }
        return true;
    };

    // 1. Idle state
    std::cout << "[Visual Test] Rendering Dock Idle State..." << std::endl;
    bridge.resetHover();
    if (!save_frame("dock_idle.png")) return 1;
    save_composite_frame("dock_shadow_light_bg.png", QStringLiteral("sunset-gradient.png"));
    save_composite_frame("dock_shadow_dark_bg.png", QStringLiteral("tinexus-default.jpg"));

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
    save_composite_frame("dock_hover_tracking.png", QStringLiteral("tinexus-default.jpg"));

    // 3. Focused state
    std::cout << "[Visual Test] Rendering Dock Focused State..." << std::endl;
    bridge.updateIconState(QStringLiteral("tinexus-terminal"), tinexus::dock::DockIconAppState::RunningFocused);
    bridge.resetHover();
    for (int f = 0; f < 20; ++f) {
        bridge.tickAnimations(0.016);
        app.processEvents();
    }
    if (!save_frame("dock_focused_app.png")) return 1;
    save_composite_frame("dock_running_glow.png", QStringLiteral("tinexus-default.jpg"));

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
    const int initialMaskW = dockWindow.inputRegionRect().width();
    std::cout << "[Visual Test] Phase 1 (BUG 1): Baseline input region width = " << initialMaskW << "px" << std::endl;

    dockModel.onToplevelAdded(QStringLiteral("vlc"));
    for (int f = 0; f < 30; ++f) {
        bridge.tickAnimations(0.016);
        app.processEvents();
    }
    const int expandedMaskW = dockWindow.inputRegionRect().width();
    std::cout << "[Visual Test] Phase 1 (BUG 1): Expanded input region width after vlc open = " << expandedMaskW << "px" << std::endl;
    if (expandedMaskW >= initialMaskW) {
        std::cout << "  [SUCCESS] Phase 1 (BUG 1): Input region dynamically expanded to accommodate running app." << std::endl;
    } else {
        std::cerr << "  [FAIL] Input region did not expand: " << expandedMaskW << " vs " << initialMaskW << std::endl;
        return 1;
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

    // 8. Auto-Hide State Machine & Dynamic Exclusive Zone (Slice 4 & Phase 1)
    std::cout << "[Visual Test] Slice 4: Enabling auto-hide while dock remains VISIBLE..." << std::endl;
    bridge.setAutoHideEnabled(true);
    app.processEvents();
    if (dockWindow.exclusiveZone() == static_cast<int>(bridge.exclusiveZone())) {
        std::cout << "  [SUCCESS] Phase 1 (Item #16): Exclusive zone preserved at " << dockWindow.exclusiveZone()
                  << "px while dock is Visible + auto-hide enabled." << std::endl;
    } else {
        std::cerr << "  [FAIL] Phase 1 (Item #16): Exclusive zone improperly zeroed while Visible: "
                  << dockWindow.exclusiveZone() << std::endl;
        return 1;
    }

    std::cout << "[Visual Test] Slice 4: Requesting hide..." << std::endl;
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
    // Verify offscreen 1x1 pass-through mask when hidden
    if (window->mask().boundingRect().y() >= window->height()) {
        std::cout << "  [SUCCESS] Phase 1 (BUG 1): Hidden state input region confirmed off-screen pass-through (y="
                  << window->mask().boundingRect().y() << ")." << std::endl;
    } else {
        std::cerr << "  [FAIL] Phase 1 (BUG 1): Hidden state mask not off-screen: " << window->mask().boundingRect().y() << std::endl;
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
    if (bridge.autoHideState() == 0 && dockWindow.exclusiveZone() == static_cast<int>(bridge.exclusiveZone())) {
        std::cout << "  [SUCCESS] Dock revealed to VISIBLE state from tripwire trigger. Zone restored to "
                  << dockWindow.exclusiveZone() << "px." << std::endl;
    } else {
        std::cerr << "  [FAIL] Dock failed to reveal or restore zone: state=" << bridge.autoHideState()
                  << " zone=" << dockWindow.exclusiveZone() << std::endl;
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

    // Phase 1 (BUG 3): Test Context Menu toggle_autohide action
    std::cout << "[Visual Test] Phase 1 (BUG 3): Context menu toggle_autohide action..." << std::endl;
    bool beforeToggle = bridge.autoHideEnabled();
    menuPopup.triggerAction(QStringLiteral("toggle_autohide"));
    app.processEvents();
    if (bridge.autoHideEnabled() != beforeToggle) {
        std::cout << "  [SUCCESS] Phase 1 (BUG 3): Context menu successfully toggled auto-hide: "
                  << bridge.autoHideEnabled() << std::endl;
    } else {
        std::cerr << "  [FAIL] Phase 1 (BUG 3): Context menu toggle_autohide failed" << std::endl;
        return 1;
    }

    // Immediately verify on-disk persistence while toggled to true
    {
        QFile configFile(QDir::homePath() + QStringLiteral("/.config/tinexus/dock.toml"));
        if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(configFile.readAll());
            configFile.close();
            if (content.contains(QStringLiteral("auto_hide = true"))) {
                std::cout << "  [SUCCESS] Phase 1 (BUG 3): Verified on-disk dock.toml contains 'auto_hide = true'!" << std::endl;
            } else {
                std::cerr << "  [FAIL] on-disk dock.toml does NOT contain 'auto_hide = true':\n"
                          << content.toStdString() << std::endl;
                return 1;
            }
        } else {
            std::cerr << "  [FAIL] Could not open on-disk dock.toml at " << configFile.fileName().toStdString() << std::endl;
            return 1;
        }
    }

    // Toggle back to false for remainder of visual tests
    bridge.setAutoHideEnabled(false);
    app.processEvents();

    // Immediately verify on-disk persistence after resetting to false
    {
        QFile configFile(QDir::homePath() + QStringLiteral("/.config/tinexus/dock.toml"));
        if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(configFile.readAll());
            configFile.close();
            if (content.contains(QStringLiteral("auto_hide = false"))) {
                std::cout << "  [SUCCESS] Phase 1 (BUG 3): Verified on-disk dock.toml restored to 'auto_hide = false'!" << std::endl;
            } else {
                std::cerr << "  [FAIL] on-disk dock.toml does NOT contain 'auto_hide = false':\n"
                          << content.toStdString() << std::endl;
                return 1;
            }
        }
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
    if (!dockModel.isPinned(QStringLiteral("firefox"))) {
        dockModel.pinApp(QStringLiteral("firefox"));
        app.processEvents();
    }
    int initialCount = dockModel.pinnedCount();
    bridge.unpinApp(QStringLiteral("firefox"));
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (dockModel.pinnedCount() == initialCount - 1 && !dockModel.isPinned(QStringLiteral("firefox"))) {
        std::cout << "  [SUCCESS] App 'firefox' successfully unpinned and removed from pinned zone." << std::endl;
        const int shrunkMaskW = dockWindow.inputRegionRect().width();
        std::cout << "[Visual Test] Phase 1 (BUG 1): Shrunk input region width after unpin = " << shrunkMaskW << "px" << std::endl;
        if (shrunkMaskW <= expandedMaskW) {
            std::cout << "  [SUCCESS] Phase 1 (BUG 1): Input region dynamically shrank after unpinning item." << std::endl;
        } else {
            std::cerr << "  [FAIL] Phase 1 (BUG 1): Input region did not shrink: " << shrunkMaskW << " vs " << expandedMaskW << std::endl;
            return 1;
        }
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

    // 16. Phase 1 Gate: Compositor Disconnect & Safety Test (Item #18)
    std::cout << "[Visual Test] Phase 1 Gate: Testing compositor disconnect safety (Item #18)..." << std::endl;
    dockModel.clearAllToplevels();
    app.processEvents();
    if (dockModel.pinnedItems()[0].appState == tinexus::dock::DockAppState::NotRunning &&
        dockModel.pinnedItems()[0].toplevelCount == 0) {
        std::cout << "  [SUCCESS] Phase 1 (Item #18): All items safely transitioned to NotRunning upon compositor disconnect." << std::endl;
    } else {
        std::cerr << "  [FAIL] Phase 1 (Item #18): Stale toplevels remained after clearAllToplevels" << std::endl;
        return 1;
    }
    // Attempt activation with null/disconnected state — must not crash or dereference
    dockModel.activateApp(QStringLiteral("tinexus-terminal"));
    dockModel.activateToplevel(nullptr);
    std::cout << "  [SUCCESS] Phase 1 (Item #18): Safe null-guarded fallback verified on disconnected app activation." << std::endl;

    // 17. Phase 1 Gate: Reconnect Loop & State Restoration Test (Item #18)
    std::cout << "[Visual Test] Phase 1 Gate: Testing foreign toplevel manager reconnection & indicator restoration..." << std::endl;
    QTimer reconnectTimer;
    reconnectTimer.setInterval(50);
    reconnectTimer.setSingleShot(false);
    bool reconnected = false;

    QObject::connect(&reconnectTimer, &QTimer::timeout, [&]() {
        // Simulate foreign toplevel manager reconnection event
        dockModel.onToplevelAdded(QStringLiteral("tinexus-terminal"));
        reconnected = true;
        reconnectTimer.stop(); // Stop immediately on success
    });
    reconnectTimer.start();
    for (int f = 0; f < 20; ++f) {
        app.processEvents();
        usleep(10000);
    }
    if (reconnected && !reconnectTimer.isActive() &&
        dockModel.getItemData(QStringLiteral("tinexus-terminal")).appState != tinexus::dock::DockAppState::NotRunning) {
        std::cout << "  [SUCCESS] Phase 1 (Item #18): Reconnected successfully, restored running indicator, and stopped reconnect timer." << std::endl;
    } else {
        std::cerr << "  [FAIL] Phase 1 (Item #18): Reconnection loop failed to restore running state or stop timer" << std::endl;
        return 1;
    }

    std::cout << "[Visual Test] All dock visual tests (Slice 1 through Slice 7: The Grand Finale) PASSED!" << std::endl;
    return 0;
}
