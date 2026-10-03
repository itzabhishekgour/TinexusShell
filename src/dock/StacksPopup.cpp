// ============================================================================
// StacksPopup.cpp — Layer-Shell Overlay Surface for Stacks Popover (Slice 7)
// Ref: Architecture Blueprint §8, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/StacksPopup.hpp"
#include "dock/StacksModel.hpp"
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

StacksPopup::StacksPopup(StacksModel* model, QObject* parent)
    : QObject(parent), m_model(model)
{
}

StacksPopup::~StacksPopup() {
    if (m_window) {
        m_window->hide();
        m_window->deleteLater();
        m_window = nullptr;
    }
}

bool StacksPopup::init(QQmlApplicationEngine* engine) {
    if (!engine) {
        tinexus::log::error("[StacksPopup] QQmlApplicationEngine is nullptr");
        return false;
    }

    engine->rootContext()->setContextProperty(QStringLiteral("stacksPopup"), this);
    if (m_model) {
        engine->rootContext()->setContextProperty(QStringLiteral("stacksModel"), m_model);
    }

    // Locate StacksPopover.qml
    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/StacksPopover.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/dock/qml/StacksPopover.qml"),
        QStringLiteral("src/dock/qml/StacksPopover.qml"),
        QStringLiteral("/workspace/src/dock/qml/StacksPopover.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/dock/qml/StacksPopover.qml"),
        QStringLiteral("/usr/share/tinexus/dock/qml/StacksPopover.qml"),
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        tinexus::log::error("[StacksPopup] Could not locate StacksPopover.qml");
        return false;
    }

    tinexus::log::info("[StacksPopup] Loading Stacks Popover from '{}'", qmlPath.toStdString());
    QQmlComponent component(engine, QUrl::fromLocalFile(qmlPath));
    if (component.isError()) {
        tinexus::log::error("[StacksPopup] Component compile errors: {}",
                            component.errorString().toStdString());
        return false;
    }

    auto* obj = component.create(engine->rootContext());
    m_window = qobject_cast<QQuickWindow*>(obj);
    if (!m_window) {
        tinexus::log::error("[StacksPopup] Root object in StacksPopover.qml is not a QQuickWindow");
        if (obj) delete obj;
        return false;
    }

    if (auto* scr = m_window->screen()) {
        const QRect geom = scr->geometry();
        m_screenWidth  = geom.width() > 0 ? geom.width() : 1920.0;
        m_screenHeight = geom.height() > 0 ? geom.height() : 1080.0;
        m_dpr          = scr->devicePixelRatio();
    }

    setupLayerShell();
    m_window->setVisible(false);
    tinexus::log::info("[StacksPopup] Initialized successfully on LayerOverlay.");
    return true;
}

void StacksPopup::setupLayerShell() {
    if (!m_window) return;

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsWin = LayerShellQt::Window::get(m_window);
    if (lsWin) {
        lsWin->setLayer(LayerShellQt::Window::LayerOverlay);
        lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(
            LayerShellQt::Window::AnchorTop    |
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft   |
            LayerShellQt::Window::AnchorRight));
        lsWin->setExclusiveZone(-1);
        lsWin->setScope(QStringLiteral("dock-stacks"));
        lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        tinexus::log::info("[StacksPopup] LayerShell configured: Layer=OVERLAY, Scope=dock-stacks");
    } else {
        tinexus::log::warn("[StacksPopup] LayerShellQt::Window::get returned null; fallback mode");
    }
#else
    tinexus::log::warn("[StacksPopup] LayerShellQt not linked; fallback frameless window mode");
#endif
}

void StacksPopup::updateScreenGeometry(double w, double h) {
    if (w > 0.0 && h > 0.0) {
        m_screenWidth  = w;
        m_screenHeight = h;
        if (m_window) {
            m_dpr = m_window->devicePixelRatio();
        }
        emit geometryChanged();
    }
}

void StacksPopup::showPopup(double screenCenterX, double targetTopY, const QString& path, const QString& title) {
    if (!path.isEmpty() && m_model) {
        m_model->setDirectoryPath(path);
    } else if (m_model && m_model->directoryPath().isEmpty()) {
        m_model->setDirectoryPath(QStringLiteral("~/Downloads"));
    }

    m_targetCenterX = screenCenterX;
    m_folderTitle   = !title.isEmpty() ? title : (m_model ? m_model->folderName() : QStringLiteral("Downloads"));

    // Center popover horizontally on icon, clamp to screen borders
    m_popupX = std::clamp(screenCenterX - (DEFAULT_POPUP_WIDTH / 2.0),
                          16.0,
                          m_screenWidth - DEFAULT_POPUP_WIDTH - 16.0);

    // Place popover vertically above dock icon with 10px spacing
    m_popupY = targetTopY - DEFAULT_POPUP_HEIGHT - 10.0;
    if (m_popupY < 24.0) {
        m_popupY = 24.0;
    }

    tinexus::log::info("[StacksPopup] Opening Stacks popover at logical=({:.1f}, {:.1f}) for '{}'",
                       m_popupX, m_popupY, m_folderTitle.toStdString());

    emit targetChanged();
    emit geometryChanged();

    m_isOpen = true;
    emit openStateChanged(true);

    if (m_window) {
        m_window->setVisible(true);
        m_window->raise();
        m_window->requestActivate();
    }
}

void StacksPopup::hidePopup() {
    if (!m_isOpen) return;

    tinexus::log::info("[StacksPopup] Hiding Stacks popover");
    m_isOpen = false;
    emit openStateChanged(false);

    if (m_window) {
        m_window->setVisible(false);
    }
}

void StacksPopup::togglePopup(double screenCenterX, double targetTopY, const QString& path, const QString& title) {
    if (m_isOpen) {
        hidePopup();
    } else {
        showPopup(screenCenterX, targetTopY, path, title);
    }
}

} // namespace tinexus::dock

#include "moc_StacksPopup.cpp"
