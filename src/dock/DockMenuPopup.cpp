// ============================================================================
// DockMenuPopup.cpp — Layer-Shell Context Menu Overlay for tinexus-dock (Slice 3)
// ============================================================================
#include "dock/DockMenuPopup.hpp"
#include <common/logger.hpp>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <algorithm>
#include <cmath>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

namespace tinexus::dock {

DockMenuPopup::DockMenuPopup(QObject* parent)
    : QObject(parent)
{
}

DockMenuPopup::~DockMenuPopup() {
    if (m_window) {
        m_window->hide();
        m_window->deleteLater();
        m_window = nullptr;
    }
}

bool DockMenuPopup::init(QQmlApplicationEngine* engine) {
    if (!engine) {
        tinexus::log::error("[DockMenuPopup] QQmlApplicationEngine is nullptr");
        return false;
    }

    // Register popup bridge to QML context before loading component
    engine->rootContext()->setContextProperty(QStringLiteral("menuPopup"), this);

    // Locate DockContextMenu.qml
    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/DockContextMenu.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/DockContextMenu.qml"),
        QStringLiteral("src/dock/qml/DockContextMenu.qml"),
        QStringLiteral("/workspace/src/dock/qml/DockContextMenu.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/dock/qml/DockContextMenu.qml"),
        QStringLiteral("/usr/share/tinexus/dock/qml/DockContextMenu.qml"),
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        tinexus::log::error("[DockMenuPopup] Could not locate DockContextMenu.qml");
        return false;
    }

    tinexus::log::info("[DockMenuPopup] Loading context menu from '{}'", qmlPath.toStdString());
    QQmlComponent component(engine, QUrl::fromLocalFile(qmlPath));
    if (component.isError()) {
        tinexus::log::error("[DockMenuPopup] Component compile errors: {}",
                            component.errorString().toStdString());
        return false;
    }

    auto* obj = component.create(engine->rootContext());
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        tinexus::log::error("[DockMenuPopup] Root QML object in DockContextMenu.qml is not a QQuickWindow");
        if (obj) delete obj;
        return false;
    }

    // Screen geometry detection
    if (auto* scr = m_window->screen()) {
        const QRect geom = scr->geometry();
        m_screenWidth  = geom.width() > 0 ? geom.width() : 1920.0;
        m_screenHeight = geom.height() > 0 ? geom.height() : 1080.0;
        m_dpr          = scr->devicePixelRatio();
    }

    setupLayerShell();

    // Context menu starts hidden until invoked
    m_window->setVisible(false);
    tinexus::log::info("[DockMenuPopup] Initialized successfully. Surface on LayerOverlay ready.");
    return true;
}

void DockMenuPopup::setupLayerShell() {
    if (!m_window) return;

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsWin = LayerShellQt::Window::get(m_window);
    if (lsWin) {
        // Decision B: Full secondary surface on LayerOverlay
        lsWin->setLayer(LayerShellQt::Window::LayerOverlay);

        // Span entire screen so outside clicks are reliably caught and dismissed
        lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(
            LayerShellQt::Window::AnchorTop    |
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft   |
            LayerShellQt::Window::AnchorRight));

        // Context menu overlay does NOT claim an exclusive zone
        lsWin->setExclusiveZone(-1);

        // Scope "dock-menu" for tinexus_blur_v1 protocol
        lsWin->setScope(QStringLiteral("dock-menu"));

        // Interactivity OnDemand gives keyboard focus for Esc key dismissal
        lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);

        tinexus::log::info("[DockMenuPopup] LayerShell configured: Layer=OVERLAY, Scope=dock-menu, Keyboard=OnDemand");
    } else {
        tinexus::log::warn("[DockMenuPopup] LayerShellQt::Window::get returned nullptr; running in standard frameless popup mode");
    }
#else
    tinexus::log::warn("[DockMenuPopup] LayerShellQt not linked; fallback frameless window mode");
#endif
}

void DockMenuPopup::updateScreenGeometry(double w, double h) {
    if (w > 0.0 && h > 0.0 && (m_screenWidth != w || m_screenHeight != h)) {
        m_screenWidth  = w;
        m_screenHeight = h;
        if (m_window) {
            m_dpr = m_window->devicePixelRatio();
        }
        emit geometryChanged();
    }
}

void DockMenuPopup::computeCoordinates(double screenCenterX, double targetTopY) {
    if (m_window) {
        m_dpr = m_window->devicePixelRatio();
    }
    if (m_dpr <= 0.0) m_dpr = 1.0;

    // Estimate menu height based on content to ensure proper vertical clearance
    double estimatedH = 46.0; // Header and inner paddings
    if (m_targetKind == static_cast<int>(DockItemKind::TrashCan)) {
        estimatedH += 32.0 * 2.0 + 12.0; // Open Trash + Empty Trash + separator
    } else if (m_targetKind == static_cast<int>(DockItemKind::Stack)) {
        estimatedH += 32.0 * 3.0 + 12.0; // Open Folder + View Modes + Remove
    } else {
        // App item
        if (m_isRunning) {
            const int winRows = std::min(4, static_cast<int>(m_windowsList.size()));
            estimatedH += winRows * 28.0;
            estimatedH += 32.0 * 2.0; // Show All Windows + Hide
            estimatedH += 12.0;       // Separator
            estimatedH += 32.0;       // Keep in Dock
            estimatedH += 12.0;       // Separator
            estimatedH += 32.0 * 2.0; // Quit + Force Quit
        } else {
            estimatedH += 32.0;       // Open
            estimatedH += 12.0;       // Separator
            estimatedH += 32.0;       // Keep in Dock
            estimatedH += 32.0;       // Show in Files
        }
    }
    m_menuHeight = estimatedH + 16.0;

    // Compute clamped logical coordinates
    m_menuX = std::clamp(screenCenterX - (DEFAULT_MENU_WIDTH / 2.0),
                         16.0,
                         m_screenWidth - DEFAULT_MENU_WIDTH - 16.0);

    m_menuY = targetTopY - m_menuHeight;
    if (m_menuY < 24.0) {
        m_menuY = 24.0;
    }

    // HiDPI physical coordinate mapping
    m_physicalX = m_menuX * m_dpr;
    m_physicalY = m_menuY * m_dpr;

    tinexus::log::info("[DockMenuPopup] Positioned menu at logical=({:.1f}, {:.1f}) physical=({:.1f}, {:.1f}) dpr={:.2f}",
                       m_menuX, m_menuY, m_physicalX, m_physicalY, m_dpr);

    emit geometryChanged();
}

void DockMenuPopup::showMenu(int iconIndex, double screenCenterX, double targetTopY,
                             const DockItemData& item, bool isPinned)
{
    m_targetIndex       = iconIndex;
    m_targetAppId       = item.appId;
    m_targetDisplayName = item.displayName;
    m_targetExec        = item.execCmd;
    m_targetIconType    = item.iconType;
    m_targetKind        = static_cast<int>(item.kind);
    m_isRunning         = (item.toplevelCount > 0 || item.appState != DockAppState::NotRunning);
    m_isActive          = (item.appState == DockAppState::RunningFocused);
    m_isPinned          = isPinned;
    m_toplevelCount     = item.toplevelCount;

    // Populate windowsList for QML
    m_windowsList.clear();
    for (const auto& t : item.toplevels) {
        QVariantMap wMap;
        wMap[QStringLiteral("title")]       = t.title.isEmpty() ? item.displayName : t.title;
        wMap[QStringLiteral("isActivated")] = t.isActivated;
        wMap[QStringLiteral("isMinimized")] = t.isMinimized;
        wMap[QStringLiteral("isMaximized")] = t.isMaximized;
        wMap[QStringLiteral("handle")]      = reinterpret_cast<quintptr>(t.handle);
        m_windowsList.append(wMap);
    }

    computeCoordinates(screenCenterX, targetTopY);

    emit targetChanged();

    m_isOpen = true;
    emit openStateChanged();

    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }

    tinexus::log::info("[DockMenuPopup] Menu opened for app='{}' kind={} isRunning={} isPinned={} windows={}",
                       m_targetAppId.toStdString(), m_targetKind, m_isRunning, m_isPinned, m_windowsList.size());
}

void DockMenuPopup::hideMenu() {
    if (!m_isOpen) return;

    m_isOpen = false;
    emit openStateChanged();
    emit dismissed();

    if (m_window) {
        m_window->hide();
    }

    tinexus::log::info("[DockMenuPopup] Menu dismissed");
}

void DockMenuPopup::triggerAction(const QString& action, const QVariantMap& params) {
    const QString appId = m_targetAppId;
    tinexus::log::info("[DockMenuPopup] Action '{}' triggered for app '{}'",
                       action.toStdString(), appId.toStdString());

    hideMenu();
    emit actionTriggered(action, appId, params);
}

} // namespace tinexus::dock

#include "moc_DockMenuPopup.cpp"
