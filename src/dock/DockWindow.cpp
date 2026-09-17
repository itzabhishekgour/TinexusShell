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
            event->type() == QEvent::MouseMove) {
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

        tinexus::log::info("[DockWindow] MainWindow LayerShell configured: Layer=TOP, Scope=dock, ExclusiveZone={}",
                           m_currentExclusiveZone);
    } else {
        tinexus::log::warn("[DockWindow] LayerShellQt::Window::get returned nullptr for mainWindow");
    }
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
        // Enabled: if hidden, exclusive zone is 0
        if (m_autoHideState == AutoHideState::Hidden) {
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
        if (m_autoHideEnabled && m_tripwireWindow) {
            m_tripwireWindow->show();
        }
    } else if (m_autoHideState == AutoHideState::Visible) {
        applyExclusiveZone(m_autoHideEnabled ? 0 : m_baseExclusiveZone);
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
    if (!m_autoHideEnabled && m_autoHideState == AutoHideState::Visible) {
        applyExclusiveZone(m_baseExclusiveZone);
    }
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

} // namespace tinexus::dock

#include "moc_DockWindow.cpp"
