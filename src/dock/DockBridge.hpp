// ============================================================================
// DockBridge.hpp — C++20 QObject Bridge for tinexus-dock
// Ref: 05_UI_UX_GUIDELINES.md §5, 06_COMPONENT_DESIGN.md §8
// Reuses: SpringStatePortable.hpp (Gate 3), IpcBridge pattern (Gate 2)
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
    QString iconType; // "terminal", "folder", "gear", "barchart", "package"
    DockIconAppState appState{DockIconAppState::NotRunning};

    tinexus::animation::SpringState scaleSpring{};
    tinexus::animation::SpringState bounceSpring{};

    double centerX{0.0};

    DockIconItem(QString id, QString lbl, QString ex, QString icn, DockIconAppState st)
        : appId(std::move(id)), label(std::move(lbl)), exec(std::move(ex)), iconType(std::move(icn)), appState(st) {}
};

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

    // QML Invokables
    Q_INVOKABLE void handleHover(double mouseX);
    Q_INVOKABLE void resetHover();
    Q_INVOKABLE void onIconClicked(int index);
    Q_INVOKABLE void updateLayout(double totalWindowWidth);

    // C++ API for external states & test harness
    void updateIconState(const QString& appId, DockIconAppState state);
    void tickAnimations(double dt);
    const std::vector<DockIconItem>& rawIcons() const { return m_icons; }

signals:
    void iconsChanged();
    void hoverChanged();
    void layoutChanged();
    void reducedMotionChanged();

private slots:
    void onAnimationTimer();
    void onSocketReadable();

private:
    void setupIpc();
    void sendIpc(uint16_t msgType, const QString& appId);
    void spawnApp(const QString& execCmd);
    void recomputeLayout();

    std::vector<DockIconItem> m_icons;
    int    m_hoveredIndex{-1};
    double m_mouseX{-1.0};
    double m_windowWidth{800.0};
    double m_pillWidth{0.0};
    bool   m_reducedMotion{false};

    QTimer m_animTimer;
    int    m_ipcFd{-1};
    std::unique_ptr<QSocketNotifier> m_notifier;
};

} // namespace tinexus::dock
