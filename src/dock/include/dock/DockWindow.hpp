// ============================================================================
// DockWindow.hpp — Layer-Shell Host & Auto-Hide Controller for tinexus-dock
// Slice 4: Fisheye Magnification & Auto-Hide
// Manages LayerShell exclusive zone adjustments and the bottom-edge tripwire.
// Ref: Architecture Blueprint §1.1, §5.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlApplicationEngine>

namespace tinexus::dock {

enum class AutoHideState : int {
    Visible = 0,
    Peaking = 1,
    Hidden  = 2
};

class DockWindow : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool autoHideEnabled READ autoHideEnabled WRITE setAutoHideEnabled NOTIFY autoHideEnabledChanged)
    Q_PROPERTY(int autoHideState READ autoHideStateInt WRITE setAutoHideStateInt NOTIFY autoHideStateChanged)
    Q_PROPERTY(int exclusiveZone READ exclusiveZone NOTIFY exclusiveZoneChanged)

public:
    explicit DockWindow(QObject* parent = nullptr);
    ~DockWindow() override;

    bool init(QQuickWindow* mainWindow, QQmlApplicationEngine* engine, int baseExclusiveZone = 72);

    [[nodiscard]] bool autoHideEnabled() const { return m_autoHideEnabled; }
    void setAutoHideEnabled(bool enabled);

    [[nodiscard]] AutoHideState autoHideState() const { return m_autoHideState; }
    [[nodiscard]] int autoHideStateInt() const { return static_cast<int>(m_autoHideState); }
    void setAutoHideState(AutoHideState state);
    void setAutoHideStateInt(int state);

    [[nodiscard]] int exclusiveZone() const { return m_currentExclusiveZone; }
    [[nodiscard]] int baseExclusiveZone() const { return m_baseExclusiveZone; }
    void setBaseExclusiveZone(int zone);

    [[nodiscard]] QQuickWindow* mainWindow() const { return m_mainWindow; }
    [[nodiscard]] QQuickWindow* tripwireWindow() const { return m_tripwireWindow; }

    // QML / Bridge Invokables
    Q_INVOKABLE void requestHide();
    Q_INVOKABLE void requestReveal();
    Q_INVOKABLE void requestPeak();
    Q_INVOKABLE void toggleAutoHide();
    Q_INVOKABLE void onTripwireEntered();
    Q_INVOKABLE void updateInputRegion(int x, int y, int width, int height);

    [[nodiscard]] QRect inputRegionRect() const { return m_inputRegionRect; }

public slots:
    void onWindowStateChanged(bool hasMaximized);

signals:
    void autoHideEnabledChanged(bool enabled);
    void autoHideStateChanged(int state);
    void exclusiveZoneChanged(int zone);
    void revealRequested();
    void hideRequested();
    void peakRequested();

private:
    void setupMainWindowLayerShell();
    void setupTripwireWindow(QQmlApplicationEngine* engine);
    void applyExclusiveZone(int zone);

    QQuickWindow* m_mainWindow{nullptr};
    QQuickWindow* m_tripwireWindow{nullptr};

    bool m_autoHideEnabled{false};
    AutoHideState m_autoHideState{AutoHideState::Visible};
    int m_baseExclusiveZone{72};
    int m_currentExclusiveZone{72};
    QRect m_inputRegionRect{};
};

} // namespace tinexus::dock
