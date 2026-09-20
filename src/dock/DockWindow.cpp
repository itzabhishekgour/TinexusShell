// ============================================================================
// DockWindow.cpp — Layer-Shell Host & Auto-Hide Controller for tinexus-dock
// ============================================================================
#include "dock/DockWindow.hpp"
#include <common/logger.hpp>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

namespace tinexus::dock {

class TripwireEventFilter : public QObject {
public:
    explicit TripwireEventFilter(DockWindow* owner) : QObject(owner), m_owner(owner) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Enter ||
            event->type() == QEvent::HoverEnter ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonPress) {
            if (m_owner) {
                m_owner->onTripwireEntered();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    DockWindow* m_owner{nullptr};
};

DockWindow::DockWindow(QObject* parent)
    : QObject(parent)
{
}

DockWindow::~DockWindow() {
    if (m_tripwireWindow) {
        m_tripwireWindow->hide();
        delete m_tripwireWindow;
        m_tripwireWindow = nullptr;
    }
}

bool DockWindow::init(QQuickWindow* mainWindow, QQmlApplicationEngine* engine, int baseExclusiveZone) {
    m_mainWindow = mainWindow;
    m_baseExclusiveZone = baseExclusiveZone;
    m_currentExclusiveZone = baseExclusiveZone;

    if (!m_mainWindow) {
        tinexus::log::error("[DockWindow] init called with null mainWindow");
        return false;
    }

    setupMainWindowLayerShell();
    setupTripwireWindow(engine);

    tinexus::log::info("[DockWindow] Initialized successfully. BaseExclusiveZone={}", m_baseExclusiveZone);
    return true;
}

void DockWindow::setupMainWindowLayerShell() {
    if (!m_mainWindow) return;

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsWin = LayerShellQt::Window::get(m_mainWindow);
    if (lsWin) {
        // Multi-monitor output affinity: bind explicitly to primary screen
        QScreen* targetScreen = m_mainWindow->screen() ? m_mainWindow->screen() : QGuiApplication::primaryScreen();
        if (targetScreen) {
            m_mainWindow->setScreen(targetScreen);
            lsWin->setScreen(targetScreen);
            lsWin->setWantsToBeOnActiveScreen(false);
        }

        // Decision A: LayerTop — dock sits above normal windows
        lsWin->setLayer(LayerShellQt::Window::LayerTop);

        // Bottom edge anchoring: full width
        lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft   |
            LayerShellQt::Window::AnchorRight));

        lsWin->setExclusiveZone(m_currentExclusiveZone);
        lsWin->setScope(QStringLiteral("dock"));
        lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

        tinexus::log::info("[DockWindow] MainWindow LayerShell configured: Layer=TOP, Scope=dock, Screen={}, ExclusiveZone={}",
                           targetScreen ? targetScreen->name().toStdString() : "primary",
                           m_currentExclusiveZone);
    } else {
        tinexus::log::warn("[DockWindow] LayerShellQt::Window::get returned nullptr for mainWindow");
    }

    // Connect primary screen change listener to maintain output affinity during hotplug
    QObject::connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen* newScreen) {
        if (!newScreen || !m_mainWindow) return;
        tinexus::log::info("[DockWindow] Primary screen changed to '{}' — updating LayerShell output affinity",
                           newScreen->name().toStdString());
        m_mainWindow->setScreen(newScreen);
        auto* win = LayerShellQt::Window::get(m_mainWindow);
        if (win) {
            win->setScreen(newScreen);
            win->setWantsToBeOnActiveScreen(false);
        }
        if (m_tripwireWindow) {
            m_tripwireWindow->setScreen(newScreen);
            m_tripwireWindow->resize(newScreen->geometry().width(), 2);
        }
    });
#else
    tinexus::log::warn("[DockWindow] LayerShellQt not linked — standard QWindow fallback");
#endif
}

void DockWindow::setupTripwireWindow(QQmlApplicationEngine*) {
    m_tripwireWindow = new QQuickWindow();
    m_tripwireWindow->setColor(Qt::transparent);
    m_tripwireWindow->setFlags(Qt::FramelessWindowHint);
    int screen_w = 1920;
    if (m_mainWindow && m_mainWindow->screen()) {
        screen_w = m_mainWindow->screen()->geometry().width();
    } else if (QGuiApplication::primaryScreen()) {
        screen_w = QGuiApplication::primaryScreen()->geometry().width();
    }
    m_tripwireWindow->resize(screen_w > 0 ? screen_w : 1920, 2);

    auto* filter = new TripwireEventFilter(this);
    m_tripwireWindow->installEventFilter(filter);

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsTrip = LayerShellQt::Window::get(m_tripwireWindow);
    if (lsTrip) {
        lsTrip->setLayer(LayerShellQt::Window::LayerTop);
        lsTrip->setAnchors(LayerShellQt::Window::Anchors::fromInt(
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft   |
            LayerShellQt::Window::AnchorRight));
        lsTrip->setExclusiveZone(0);
        lsTrip->setScope(QStringLiteral("dock-tripwire"));
        lsTrip->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

        tinexus::log::info("[DockWindow] Tripwire LayerShell configured: 2px edge surface on LayerTop");
    }
#endif

    // Tripwire starts hidden until dock enters Hidden state
    m_tripwireWindow->setVisible(false);
}

void DockWindow::applyExclusiveZone(int zone) {
    if (m_currentExclusiveZone == zone) return;
    m_currentExclusiveZone = zone;

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    if (m_mainWindow) {
        auto* lsWin = LayerShellQt::Window::get(m_mainWindow);
        if (lsWin) {
            lsWin->setExclusiveZone(zone);
            tinexus::log::info("[DockWindow] Dynamic LayerShell exclusive zone adjusted to {}", zone);
        }
    }
#endif

    emit exclusiveZoneChanged(zone);
}

void DockWindow::setAutoHideEnabled(bool enabled) {
    if (m_autoHideEnabled == enabled) return;
    m_autoHideEnabled = enabled;
    tinexus::log::info("[DockWindow] setAutoHideEnabled: {}", enabled);

    if (!m_autoHideEnabled) {
        // Disabled: dock must stay visible with full exclusive zone
        setAutoHideState(AutoHideState::Visible);
        applyExclusiveZone(m_baseExclusiveZone);
        if (m_tripwireWindow) m_tripwireWindow->hide();
    } else {
        // Enabled: if currently Visible, do NOT zero exclusive zone!
        // Windows should only overlap when the dock is actually Hidden.
        if (m_autoHideState == AutoHideState::Visible) {
            applyExclusiveZone(m_baseExclusiveZone);
            if (m_tripwireWindow) m_tripwireWindow->hide();
        } else if (m_autoHideState == AutoHideState::Hidden) {
            applyExclusiveZone(0);
            if (m_tripwireWindow) m_tripwireWindow->show();
        }
    }

    emit autoHideEnabledChanged(m_autoHideEnabled);
}

void DockWindow::setAutoHideState(AutoHideState state) {
    if (m_autoHideState == state) return;
    m_autoHideState = state;
    tinexus::log::info("[DockWindow] autoHideState changed to {}", static_cast<int>(state));

    if (m_autoHideState == AutoHideState::Hidden) {
        applyExclusiveZone(0);
        // Make main surface pass-through to pointer events
        updateInputRegion(0, 0, 0, 0);
        if (m_autoHideEnabled && m_tripwireWindow) {
            m_tripwireWindow->show();
        }
    } else if (m_autoHideState == AutoHideState::Visible) {
        // Fix: always maintain baseExclusiveZone while Visible
        applyExclusiveZone(m_baseExclusiveZone);
        if (!m_inputRegionRect.isEmpty()) {
            updateInputRegion(m_inputRegionRect.x(), m_inputRegionRect.y(),
                              m_inputRegionRect.width(), m_inputRegionRect.height());
        }
        if (m_tripwireWindow) {
            m_tripwireWindow->hide();
        }
    } else if (m_autoHideState == AutoHideState::Peaking) {
        applyExclusiveZone(0);
    }

    emit autoHideStateChanged(static_cast<int>(m_autoHideState));
}

void DockWindow::setAutoHideStateInt(int state) {
    setAutoHideState(static_cast<AutoHideState>(state));
}

void DockWindow::setBaseExclusiveZone(int zone) {
    if (m_baseExclusiveZone == zone) return;
    m_baseExclusiveZone = zone;
    tinexus::log::info("[DockWindow] setBaseExclusiveZone: {}", zone);
    if (m_autoHideState == AutoHideState::Visible) {
        applyExclusiveZone(m_baseExclusiveZone);
    }
}

void DockWindow::updateInputRegion(int x, int y, int width, int height) {
    if (!m_mainWindow) return;

    if (m_autoHideState == AutoHideState::Hidden || width <= 0 || height <= 0) {
        // Wayland protocol: passing empty wl_region to wl_surface.set_input_region
        // makes surface accept no pointer events. Qt's setMask(QRegion()) calls
        // set_input_region(nullptr) which resets to infinite/full window.
        // Therefore, we pass a 1x1 offscreen pixel region to guarantee total pass-through.
        const int offscreenY = m_mainWindow->height() > 0 ? (m_mainWindow->height() + 50) : 2000;
        const QRect offscreen(0, offscreenY, 1, 1);
        m_mainWindow->setMask(QRegion(offscreen));
        tinexus::log::info("[DockWindow] Wayland input region set to pass-through (offscreen 1x1 at y={})", offscreenY);
        return;
    }

    const QRect rect(x, y, width, height);
    m_inputRegionRect = rect;
    m_mainWindow->setMask(QRegion(rect));
    tinexus::log::info("[DockWindow] Wayland input region mask updated: QRect(x={} y={} w={} h={})",
                       x, y, width, height);
}

void DockWindow::requestHide() {
    emit hideRequested();
}

void DockWindow::requestReveal() {
    emit revealRequested();
}

void DockWindow::requestPeak() {
    emit peakRequested();
}

void DockWindow::toggleAutoHide() {
    setAutoHideEnabled(!m_autoHideEnabled);
}

void DockWindow::onTripwireEntered() {
    tinexus::log::info("[DockWindow] Pointer hit 2px tripwire edge — triggering reveal");
    requestReveal();
}

void DockWindow::onWindowStateChanged(bool hasMaximized) {
    tinexus::log::info("[DockWindow] onWindowStateChanged: hasMaximized={}", hasMaximized);
    if (hasMaximized) {
        setAutoHideEnabled(true);
        setAutoHideState(AutoHideState::Hidden);
        requestHide();
    } else {
        setAutoHideEnabled(false);
        setAutoHideState(AutoHideState::Visible);
        requestReveal();
    }
}

} // namespace tinexus::dock

#include "moc_DockWindow.cpp"
