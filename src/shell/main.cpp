// ============================================================================
// main.cpp — tinexus-shell (Qt6 / Layer-shell)
// ============================================================================
#include "ShellBridge.hpp"
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
    tinexus::log::set_component_name("shell");
    tinexus::log::info("tinexus-shell starting (Qt6)...");

    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
    QGuiApplication app(argc, argv);
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    app.setApplicationName(QStringLiteral("tinexus-shell"));
    app.setDesktopFileName(QStringLiteral("io.tinexus.shell.TopBar"));

    tinexus::shell::ShellBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DesktopShellWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("/usr/share/tinexus/shell/qml/DesktopShellWindow.qml")
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
        std::cerr << "FAIL: Failed to load root QML object for shell" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
        auto* lsWin = LayerShellQt::Window::get(window);
        if (lsWin) {
            lsWin->setLayer(LayerShellQt::Window::LayerTop);
            lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(LayerShellQt::Window::AnchorTop |
                                                                    LayerShellQt::Window::AnchorLeft |
                                                                    LayerShellQt::Window::AnchorRight));
            lsWin->setExclusiveZone(32);
            std::cout << "[tinexus-shell] LayerShellQt configured: Layer=Top, ExclusiveZone=32" << std::endl;
        }
#else
        std::cout << "[tinexus-shell] LayerShellQt not linked — running in fallback QWindow mode" << std::endl;
#endif

        // ── Dynamic input-region mask (Tiny-Dead-Zone fix) ──────────────────
        // The shell window is 46px tall (to accommodate the AuraNotch protrusion)
        // but the exclusive zone is only 32px. Without a mask, the compositor's
        // scene-graph hit-test (wlr_scene_node_at) will intercept clicks in the
        // y=32–46 band on the LEFT and RIGHT sides where there is no interactive
        // content — creating a "dead zone" below the top bar.
        //
        // QWindow::setMask(QRegion) translates to wl_surface_set_input_region()
        // via the Qt Wayland platform plugin, so only the specified rects receive
        // pointer input; all other areas are transparent to mouse events.
        //
        // Interactive regions:
        //   • Full-width flat bar : (0,   0,  width, 32)
        //   • Center notch top    : (cx-136, 32, 272,  14)  — AuraNotch trapezoid
        //
        // When any flyout is open, the mask is cleared → full 380px height is hit-testable.

        constexpr int kBarH      = 32;    // flat bar height (exclusive zone)
        constexpr int kNotchH    = 46;    // total notch height
        constexpr int kNotchHalf = 136;   // half-width of notch top edge (AuraNotch.qml)

        // Lambda: compute and apply the correct mask for the current flyout state.
        auto applyInputMask = [window, &bridge, kBarH, kNotchH, kNotchHalf]() {
            bool anyOpen = bridge.logoMenuOpen()   || bridge.appMenuOpen()       ||
                           bridge.calendarOpen()   || bridge.notificationsOpen() ||
                           bridge.volumeFlyoutOpen()|| bridge.brightnessFlyoutOpen() ||
                           bridge.rebootConfirmationOpen() || bridge.shutdownConfirmationOpen();

            if (anyOpen) {
                // Flyout open — full window height must receive input
                window->setMask(QRegion());
                tinexus::log::debug("[shell] Input region: full window (flyout open)");
            } else {
                int w  = window->width();
                int cx = w / 2;
                QRegion mask;
                mask += QRect(0,               0,    w,             kBarH);       // flat bar
                mask += QRect(cx - kNotchHalf, kBarH, kNotchHalf * 2,
                              kNotchH - kBarH);                                   // center notch
                window->setMask(mask);
                tinexus::log::debug("[shell] Input region: bar+notch only (idle)");
            }
        };

        // Apply initial idle mask
        applyInputMask();

        // Re-apply whenever flyout state changes
        QObject::connect(&bridge, &tinexus::shell::ShellBridge::flyoutStateChanged,
                         window, applyInputMask);

        // Re-apply if window width changes (e.g. dynamic output resize)
        QObject::connect(window, &QWindow::widthChanged, window,
                         [applyInputMask](int /*newW*/) { applyInputMask(); });

        window->show();
    }

    return app.exec();
}
