// ============================================================================
// DockBridge.hpp — C++20 QObject Bridge for tinexus-dock
// Ref: 05_UI_UX_GUIDELINES.md §5, 06_COMPONENT_DESIGN.md §8
// Reuses: SpringStatePortable.hpp (Gate 3), IpcBridge pattern (Gate 2)
// Slice 3: Click, Activate, Context Menu Overlay Routing
// Slice 4: Fisheye Magnification & Auto-Hide State Controller
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include <QtCore/QSocketNotifier>
#include <common/SpringStatePortable.hpp>
#include <memory>
#include <vector>
#include <string>

namespace tinexus::dock {

enum class DockIconAppState : int {
    NotRunning     = 0,
    RunningFocused = 1,
    RunningBg      = 2,
    Minimized      = 3
};

struct DockIconItem {
    QString appId;
    QString label;
    QString exec;
    QString iconType; // "terminal", "folder", "gear", "barchart", "package", "separator", "trash"
    DockIconAppState appState{DockIconAppState::NotRunning};
    int     toplevelCount{0};
    uint32_t badgeCount{0};

    tinexus::animation::SpringState scaleSpring{};
    tinexus::animation::SpringState bounceSpring{};

    double centerX{0.0};

    DockIconItem(QString id, QString lbl, QString ex, QString icn, DockIconAppState st, int topCount = 0, uint32_t badge = 0)
        : appId(std::move(id)), label(std::move(lbl)), exec(std::move(ex)), iconType(std::move(icn)),
          appState(st), toplevelCount(topCount), badgeCount(badge) {}
};

class DockModel;
class DockMenuPopup;
class DockWindow;
class StacksPopup;

class DockBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList icons READ iconsList NOTIFY iconsChanged)
    Q_PROPERTY(int hoveredIndex READ hoveredIndex NOTIFY hoverChanged)
    Q_PROPERTY(double pillWidth READ pillWidth NOTIFY layoutChanged)
    Q_PROPERTY(double pillHeight READ pillHeight CONSTANT)
    Q_PROPERTY(double baseSize READ baseSize CONSTANT)
    Q_PROPERTY(double gap READ gap CONSTANT)
    Q_PROPERTY(double dockPad READ dockPad CONSTANT)
    Q_PROPERTY(double dockBotMargin READ dockBotMargin CONSTANT)
    Q_PROPERTY(double exclusiveZone READ exclusiveZone CONSTANT)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)

    // Slice 4: Auto-Hide properties
    Q_PROPERTY(bool autoHideEnabled READ autoHideEnabled WRITE setAutoHideEnabled NOTIFY autoHideEnabledChanged)
    Q_PROPERTY(int autoHideState READ autoHideState WRITE setAutoHideState NOTIFY autoHideStateChanged)

    // Slice 6: Wayland File Drag-and-Drop
    Q_PROPERTY(int dropTargetIndex READ dropTargetIndex NOTIFY dropTargetChanged)

public:
    static constexpr double BASE_SIZE       = 40.0;
    static constexpr double GAP             = 8.0;
    static constexpr double DOCK_PAD        = 8.0;
    static constexpr double DOCK_BOT_MARGIN = 8.0;
    static constexpr double MAX_SCALE       = 1.68;
    static constexpr double INFLUENCE_R     = 3.5 * BASE_SIZE; // 140.0

    explicit DockBridge(QObject* parent = nullptr);
    ~DockBridge() override;

    [[nodiscard]] QVariantList iconsList() const;
    [[nodiscard]] int hoveredIndex() const { return m_hoveredIndex; }
    [[nodiscard]] double pillWidth() const { return m_pillWidth; }
    [[nodiscard]] double pillHeight() const { return BASE_SIZE + DOCK_PAD * 2.0; } // 56.0
    [[nodiscard]] double baseSize() const { return BASE_SIZE; }
    [[nodiscard]] double gap() const { return GAP; }
    [[nodiscard]] double dockPad() const { return DOCK_PAD; }
    [[nodiscard]] double dockBotMargin() const { return DOCK_BOT_MARGIN; }
    [[nodiscard]] double exclusiveZone() const { return DOCK_BOT_MARGIN + (BASE_SIZE + DOCK_PAD * 2.0) + 8.0; } // 72.0
    [[nodiscard]] bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool val);

    [[nodiscard]] bool autoHideEnabled() const { return m_autoHideEnabled; }
    Q_INVOKABLE void setAutoHideEnabled(bool val);
    [[nodiscard]] int autoHideState() const { return m_autoHideState; }
    Q_INVOKABLE void setAutoHideState(int val);

    [[nodiscard]] int dropTargetIndex() const { return m_dropTargetIndex; }
    Q_INVOKABLE void setDropTargetIndex(int idx);

    // QML Invokables
    Q_INVOKABLE void handleHover(double mouseX);
    Q_INVOKABLE void resetHover();
    Q_INVOKABLE void onIconClicked(int index);
    Q_INVOKABLE void activateApp(const QString& appId);
    Q_INVOKABLE void requestContextMenu(int index);
    Q_INVOKABLE void updateLayout(double totalWindowWidth);
    Q_INVOKABLE void updateScreenGeometry(double totalWindowWidth, double totalWindowHeight);
    Q_INVOKABLE void minimizeApp(const QString& appId);
    Q_INVOKABLE void closeApp(const QString& appId);
    Q_INVOKABLE void forceQuitApp(const QString& appId);

    // Slice 4: Auto-Hide invokables
    Q_INVOKABLE void requestHide();
    Q_INVOKABLE void requestReveal();
    Q_INVOKABLE void toggleAutoHide();

    // Slice 5: Drag Rearrange & Poof invokables
    Q_INVOKABLE void moveItem(int fromIndex, int toIndex);
    Q_INVOKABLE void unpinApp(const QString& appId);
    Q_INVOKABLE void commitMove();

    // Slice 6: Wayland File Drag-and-Drop invokable
    Q_INVOKABLE void launchWithUris(const QString& appId, const QStringList& uris);

    // C++ API for external states & test harness
    void attachModel(DockModel* model);
    void attachMenuPopup(DockMenuPopup* popup);
    void attachDockWindow(DockWindow* dockWindow);
    void attachStacksPopup(StacksPopup* stacksPopup);
    [[nodiscard]] DockMenuPopup* menuPopup() const { return m_menuPopup; }
    [[nodiscard]] DockWindow* dockWindow() const { return m_dockWindow; }
    [[nodiscard]] StacksPopup* stacksPopup() const { return m_stacksPopup; }
    void updateIconState(const QString& appId, DockIconAppState state);
    void tickAnimations(double dt);
    const std::vector<DockIconItem>& rawIcons() const { return m_icons; }

signals:
    void iconsChanged();
    void hoverChanged();
    void layoutChanged();
    void reducedMotionChanged();
    void autoHideEnabledChanged(bool enabled);
    void autoHideStateChanged(int state);
    void revealRequested();
    void hideRequested();
    void dropTargetChanged(int index);

public slots:
    void syncFromModel();
    void onMenuActionTriggered(const QString& action, const QString& appId, const QVariantMap& params);

private slots:
    void onAnimationTimer();
    void onSocketReadable();

private:
    void setupIpc();
    void sendIpc(uint16_t msgType, const QString& appId);
    void spawnApp(const QString& execCmd);
    void recomputeLayout();

    std::vector<DockIconItem> m_icons;
    DockModel* m_model{nullptr};
    DockMenuPopup* m_menuPopup{nullptr};
    DockWindow* m_dockWindow{nullptr};
    StacksPopup* m_stacksPopup{nullptr};

    int    m_hoveredIndex{-1};
    int    m_dropTargetIndex{-1};
    double m_mouseX{-1.0};
    double m_windowWidth{800.0};
    double m_screenHeight{1080.0};
    double m_pillWidth{0.0};
    bool   m_reducedMotion{false};
    bool   m_autoHideEnabled{false};
    int    m_autoHideState{0}; // 0=Visible, 1=Peaking, 2=Hidden

    QTimer m_animTimer;
    int    m_ipcFd{-1};
    std::unique_ptr<QSocketNotifier> m_notifier;
};

} // namespace tinexus::dock
