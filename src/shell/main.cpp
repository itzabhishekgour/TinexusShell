// ============================================================================
// main.cpp — tinexus-shell (Qt6 / Layer-shell)
// ============================================================================
#include "ShellBridge.hpp"
#include "TinexusIconProvider.hpp"
#include <common/logger.hpp>
#include <common/SingleInstance.hpp>
#include <common/DBusNames.hpp>
#include <QtGui/QGuiApplication>
#include <QtGui/QFontDatabase>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItem>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>
#include <cmath>

#include <QtCore/QEvent>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

namespace {
class ShellFocusFilter : public QObject {
public:
    ShellFocusFilter(QQuickWindow* win, tinexus::shell::ShellBridge* bridge)
        : m_win(win), m_bridge(bridge) {}
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::FocusOut || ev->type() == QEvent::ActivationChange) {
            if (m_win && !m_win->isActive() && m_bridge) {
                bool anyOpen = m_bridge->logoMenuOpen() || m_bridge->appMenuOpen() ||
                               m_bridge->calendarOpen() || m_bridge->notificationsOpen() ||
                               m_bridge->volumeFlyoutOpen() || m_bridge->brightnessFlyoutOpen() ||
                               m_bridge->rebootConfirmationOpen() || m_bridge->shutdownConfirmationOpen();
                if (anyOpen) {
                    tinexus::log::debug("[shell] Focus lost — auto-dismissing active flyout");
                    m_bridge->closeAllFlyouts();
                }
            }
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    QQuickWindow* m_win{nullptr};
    tinexus::shell::ShellBridge* m_bridge{nullptr};
};
} // namespace

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("shell");
    tinexus::log::info("tinexus-shell starting (Qt6)...");

    tinexus::common::SingleInstance single_instance("tinexus-shell");
    if (!single_instance.is_primary()) {
        tinexus::log::warn("[shell] Another instance of tinexus-shell is already running; exiting secondary instance.");
        return 0;
    }

    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
    QGuiApplication app(argc, argv);
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    app.setApplicationName(QStringLiteral("tinexus-shell"));
    app.setDesktopFileName(tinexus::common::dbus::qapp_id::TopBar());

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

    tinexus::shell::ShellBridge bridge;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new tinexus::shell::TinexusIconProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QStringLiteral("/workspace/src/shell/qml/DesktopShellWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DesktopShellWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/shell/qml/DesktopShellWindow.qml"),
        QStringLiteral("src/shell/qml/DesktopShellWindow.qml"),
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

        // ── Dynamic input-region mask (Precise Compound Geometry) ───────────
        // The shell window dynamically expands up to 420px height when a flyout
        // opens. We construct an exact compound QRegion containing ONLY:
        //   1. The 32px baseline top bar
        //   2. The AuraNotch area (either 272x46 idle, or 420x72 expanded)
        //   3. The exact bounding box of whichever flyout is currently open,
        //      queried dynamically from the QML item scene mapping to prevent drift.
        //
        // All other transparent areas remain excluded from the Wayland input
        // region mask, allowing pointer events to pass cleanly to underlying windows.

        constexpr int kBarH      = 32;    // flat bar height (exclusive zone)
        constexpr int kNotchH    = 46;    // total notch height
        constexpr int kNotchHalf = 136;   // half-width of notch top edge (AuraNotch.qml)

        // Lambda: compute and apply the exact compound mask for current state
        auto applyInputMask = [window, &bridge, kBarH, kNotchH, kNotchHalf]() {
            int w  = window->width();
            int cx = w / 2;
            QRegion mask;

            // 1. Always mask the top bar
            mask += QRect(0, 0, w, kBarH);

            // 2. Center notch geometry
            if (bridge.notchExpanded()) {
                mask += QRect(cx - 210, 0, 420, 72);
            } else {
                mask += QRect(cx - kNotchHalf, 0, kNotchHalf * 2, kNotchH);
            }

            // 3. Add dynamic geometry for open flyouts by querying actual QQuickItem positions & sizes
            auto addFlyoutItemMask = [window, &mask](const char* objName, const QRect& fallbackRect) {
                bool added = false;
                if (auto* item = window->findChild<QQuickItem*>(QString::fromLatin1(objName))) {
                    if (item->isVisible() && item->opacity() > 0.01) {
                        QPointF p = item->mapToScene(QPointF(0, 0));
                        int rx = static_cast<int>(std::floor(p.x()));
                        int ry = static_cast<int>(std::floor(p.y()));
                        int rw = static_cast<int>(std::ceil(item->width()));
                        int rh = static_cast<int>(std::ceil(item->height()));
                        if (rw > 0 && rh > 0) {
                            mask += QRect(rx, ry, rw, rh);
                            added = true;
                        }
                    }
                }
                if (!added) {
                    mask += fallbackRect;
                }
            };

            if (bridge.logoMenuOpen()) {
                addFlyoutItemMask("logoFlyout", QRect(8, 36, 230, 290));
            }
            if (bridge.appMenuOpen()) {
                addFlyoutItemMask("appsFlyout", QRect(48, 36, 350, 390));
            }
            if (bridge.calendarOpen()) {
                addFlyoutItemMask("calFlyout", QRect(cx - 90, 48, 310, 310));
            }
            if (bridge.notificationsOpen()) {
                addFlyoutItemMask("notifFlyout", QRect(w - 398, 36, 390, 370));
            }
            if (bridge.volumeFlyoutOpen()) {
                addFlyoutItemMask("volFlyout", QRect(w - 295, 36, 260, 190));
            }
            if (bridge.brightnessFlyoutOpen()) {
                addFlyoutItemMask("briFlyout", QRect(w - 325, 36, 260, 110));
            }
            if (bridge.rebootConfirmationOpen() || bridge.shutdownConfirmationOpen()) {
                addFlyoutItemMask("powerDialog", QRect(cx - 190, 60, 380, 180));
            }
            if (bridge.toastVisible()) {
                addFlyoutItemMask("notifToast", QRect(w - 370, 38, 360, 72));
            }

            window->setMask(mask);
            tinexus::log::debug("[shell] Applied dynamic compound input mask (rect count={})", mask.rectCount());
        };

        // Install event filter for auto-dismissing flyouts on focus loss
        auto* focusFilter = new ShellFocusFilter(window, &bridge);
        window->installEventFilter(focusFilter);

        // Apply initial idle mask
        applyInputMask();

        // Re-apply whenever flyout state changes
        QObject::connect(&bridge, &tinexus::shell::ShellBridge::flyoutStateChanged,
                         window, applyInputMask);

        // Re-apply whenever toast visibility changes
        QObject::connect(&bridge, &tinexus::shell::ShellBridge::toastVisibleChanged,
                         window, applyInputMask);

        // Re-apply if window width changes (e.g. dynamic output resize)
        QObject::connect(window, &QWindow::widthChanged, window,
                         [applyInputMask](int /*newW*/) { applyInputMask(); });

        window->show();
    }

    return app.exec();
}
