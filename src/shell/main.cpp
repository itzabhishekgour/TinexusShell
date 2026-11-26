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
            lsWin->setScope(QStringLiteral("tinexus-shell"));
            std::cout << "[tinexus-shell] LayerShellQt configured: Layer=Top, ExclusiveZone=32, Scope=tinexus-shell" << std::endl;
        }
#else
        std::cout << "[tinexus-shell] LayerShellQt not linked — running in fallback QWindow mode" << std::endl;
#endif

        bridge.setShellWindow(window);

        // ── Dynamic input-region mask (Precise Compound Geometry) ───────────
        // The shell window dynamically expands up to 420px height when a flyout
        // opens. We construct an exact compound QRegion containing ONLY:
        //   1. The 32px baseline top bar (which already encloses the 30px idle notch)
        //   2. Expanded AuraNotch area (420x68 when expanded or animating > 32px)
        //   3. The exact bounding box of whichever flyout is currently visible/animating,
        //      queried dynamically from the QML item scene mapping to prevent drift.
        //
        // Both Wayland input_region (window->setMask) and org_kde_kwin_blur region stay
        // synchronized so blur & pointer routing follow animated flyouts cleanly.

        constexpr int kBarH = 32; // flat bar height (exclusive zone)

        // Lambda: compute and apply the exact compound mask for current state
        auto applyInputMask = [window, &bridge, kBarH]() {
            int w  = window->width();
            int cx = w / 2;
            QRegion mask;

            // 1. Always mask the top bar (also covers the 236x30 idle AuraNotch at y=1)
            mask += QRect(0, 0, w, kBarH);

            // 2. Center notch geometry only extends below bar when expanded/animating
            if (bridge.notchExpanded()) {
                mask += QRect(cx - 210, 0, 420, 70);
            } else if (auto* notchItem = window->findChild<QQuickItem*>(QStringLiteral("centerNotchPill"))) {
                if (notchItem->height() > 33.0) {
                    QPointF p = notchItem->mapToScene(QPointF(0, 0));
                    mask += QRect(static_cast<int>(std::floor(p.x())),
                                  static_cast<int>(std::floor(p.y())),
                                  static_cast<int>(std::ceil(notchItem->width())),
                                  static_cast<int>(std::ceil(notchItem->height())));
                }
            }

            // 3. Add dynamic geometry for open or closing-animation flyouts
            auto addFlyoutItemMask = [window, &mask](const char* objName, const QRect& fallbackRect, bool isOpen) {
                bool added = false;
                if (auto* item = window->findChild<QQuickItem*>(QString::fromLatin1(objName))) {
                    if (item->isVisible() && item->width() > 0.0 && item->height() > 0.0) {
                        QQuickItem* host = (item->parentItem() && item->parentItem()->parentItem())
                            ? item->parentItem()->parentItem()
                            : nullptr;
                        const double padVal = item->property("shadowPad").isValid()
                            ? item->property("shadowPad").toDouble()
                            : 0.0;
                        const double pad = std::max(0.0, padVal);
                        const double innerW = std::max(1.0, item->width() - 2.0 * pad);
                        const double innerH = std::max(1.0, item->height() - 2.0 * pad);
                        const bool hostVisible = host ? (host->opacity() > 0.005) : (item->opacity() > 0.005);
                        if (hostVisible) {
                            // mapRectToScene incorporates FlyoutHost spring scale + Translate(y)
                            // so blur and input mask track the inner glass card cleanly.
                            const QRectF sceneRect = item->mapRectToScene(QRectF(pad, pad, innerW, innerH));
                            const int rx = std::max(0, static_cast<int>(std::floor(sceneRect.x())));
                            const int ry = std::max(0, static_cast<int>(std::floor(sceneRect.y())));
                            const int rw = std::max(1, static_cast<int>(std::ceil(sceneRect.width())));
                            const int rh = std::max(1, static_cast<int>(std::ceil(sceneRect.height())));
                            mask += QRect(rx, ry, rw, rh);
                            added = true;
                        }
                    }
                }
                if (!added && isOpen) {
                    mask += fallbackRect;
                }
            };

            addFlyoutItemMask("logoFlyout",   QRect(8, 36, 224, 274),        bridge.logoMenuOpen());
            addFlyoutItemMask("appsFlyout",   QRect(48, 36, 248, 360),       bridge.appMenuOpen());
            addFlyoutItemMask("calFlyout",    QRect(cx - 90, 48, 300, 310),  bridge.calendarOpen());
            addFlyoutItemMask("notifFlyout",  QRect(w - 376, 36, 360, 182),  bridge.notificationsOpen());
            addFlyoutItemMask("volFlyout",    QRect(w - 295, 36, 256, 174),  bridge.volumeFlyoutOpen());
            addFlyoutItemMask("briFlyout",    QRect(w - 325, 36, 250, 116),  bridge.brightnessFlyoutOpen());
            addFlyoutItemMask("powerDialog",  QRect(cx - 190, 60, 380, 180), bridge.rebootConfirmationOpen() || bridge.shutdownConfirmationOpen());
            addFlyoutItemMask("notifToast",   QRect(w - 368, 40, 350, 68),   bridge.toastVisible());

            window->setMask(mask);
            window->requestUpdate();
            bridge.syncBlurRegion(mask);
            tinexus::log::debug("[shell] Applied dynamic compound input/blur mask (rect count={})", mask.rectCount());
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

        // Re-apply whenever AuraNotch expands or collapses
        QObject::connect(&bridge, &tinexus::shell::ShellBridge::notchExpandedChanged,
                         window, applyInputMask);

        // Re-apply when QML flyout loaders / spring animations settle
        QObject::connect(&bridge, &tinexus::shell::ShellBridge::overlayGeometrySyncRequested,
                         window, applyInputMask);

        // Re-apply if window width changes (e.g. dynamic output resize)
        QObject::connect(window, &QWindow::widthChanged, window,
                         [applyInputMask](int /*newW*/) { applyInputMask(); });

        window->show();
        applyInputMask();
    }

    return app.exec();
}
