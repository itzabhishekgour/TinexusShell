// ============================================================================
// DockMenuPopup.hpp — Layer-Shell Context Menu Overlay for tinexus-dock (Slice 3)
// Creates an independent zwlr_layer_shell_v1 surface on LayerOverlay to host
// DockContextMenu.qml with LiquidGlass styling, precise HiDPI screen space
// coordinates, outside-click dismissal, and keyboard accessibility.
// Ref: Architecture Blueprint §1.1, Decision B, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlApplicationEngine>
#include "dock/DockModel.hpp"

namespace tinexus::dock {

class DockMenuPopup : public QObject {
    Q_OBJECT

    Q_PROPERTY(double menuX READ menuX NOTIFY geometryChanged)
    Q_PROPERTY(double menuY READ menuY NOTIFY geometryChanged)
    Q_PROPERTY(double menuWidth READ menuWidth CONSTANT)
    Q_PROPERTY(double menuHeight READ menuHeight NOTIFY geometryChanged)
    Q_PROPERTY(double physicalX READ physicalX NOTIFY geometryChanged)
    Q_PROPERTY(double physicalY READ physicalY NOTIFY geometryChanged)
    Q_PROPERTY(double devicePixelRatio READ devicePixelRatio NOTIFY geometryChanged)
    Q_PROPERTY(double screenWidth READ screenWidth NOTIFY geometryChanged)
    Q_PROPERTY(double screenHeight READ screenHeight NOTIFY geometryChanged)

    Q_PROPERTY(QString targetAppId READ targetAppId NOTIFY targetChanged)
    Q_PROPERTY(QString targetDisplayName READ targetDisplayName NOTIFY targetChanged)
    Q_PROPERTY(QString targetExec READ targetExec NOTIFY targetChanged)
    Q_PROPERTY(QString targetIconType READ targetIconType NOTIFY targetChanged)
    Q_PROPERTY(int targetKind READ targetKind NOTIFY targetChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY targetChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY targetChanged)
    Q_PROPERTY(bool isPinned READ isPinned NOTIFY targetChanged)
    Q_PROPERTY(int toplevelCount READ toplevelCount NOTIFY targetChanged)
    Q_PROPERTY(QVariantList windowsList READ windowsList NOTIFY targetChanged)

    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openStateChanged)

public:
    static constexpr double DEFAULT_MENU_WIDTH = 220.0;

    explicit DockMenuPopup(QObject* parent = nullptr);
    ~DockMenuPopup() override;

    bool init(QQmlApplicationEngine* engine);

    [[nodiscard]] double menuX() const { return m_menuX; }
    [[nodiscard]] double menuY() const { return m_menuY; }
    [[nodiscard]] double menuWidth() const { return DEFAULT_MENU_WIDTH; }
    [[nodiscard]] double menuHeight() const { return m_menuHeight; }
    [[nodiscard]] double physicalX() const { return m_physicalX; }
    [[nodiscard]] double physicalY() const { return m_physicalY; }
    [[nodiscard]] double devicePixelRatio() const { return m_dpr; }
    [[nodiscard]] double screenWidth() const { return m_screenWidth; }
    [[nodiscard]] double screenHeight() const { return m_screenHeight; }

    [[nodiscard]] QString targetAppId() const { return m_targetAppId; }
    [[nodiscard]] QString targetDisplayName() const { return m_targetDisplayName; }
    [[nodiscard]] QString targetExec() const { return m_targetExec; }
    [[nodiscard]] QString targetIconType() const { return m_targetIconType; }
    [[nodiscard]] int targetKind() const { return m_targetKind; }
    [[nodiscard]] bool isRunning() const { return m_isRunning; }
    [[nodiscard]] bool isActive() const { return m_isActive; }
    [[nodiscard]] bool isPinned() const { return m_isPinned; }
    [[nodiscard]] int toplevelCount() const { return m_toplevelCount; }
    [[nodiscard]] QVariantList windowsList() const { return m_windowsList; }
    [[nodiscard]] bool isOpen() const { return m_isOpen; }
    [[nodiscard]] QQuickWindow* window() const { return m_window; }

    Q_INVOKABLE void updateScreenGeometry(double w, double h);

    // QML / C++ Invokables
    void showMenu(int iconIndex, double screenCenterX, double targetTopY,
                  const DockItemData& item, bool isPinned);
    Q_INVOKABLE void hideMenu();
    Q_INVOKABLE void triggerAction(const QString& action, const QVariantMap& params = {});

signals:
    void geometryChanged();
    void targetChanged();
    void openStateChanged();
    void actionTriggered(const QString& action, const QString& appId, const QVariantMap& params);
    void dismissed();

private:
    void setupLayerShell();
    void computeCoordinates(double screenCenterX, double targetTopY);

    QQuickWindow* m_window{nullptr};
    bool m_isOpen{false};

    double m_menuX{100.0};
    double m_menuY{100.0};
    double m_menuHeight{200.0};
    double m_physicalX{100.0};
    double m_physicalY{100.0};
    double m_dpr{1.0};
    double m_screenWidth{1920.0};
    double m_screenHeight{1080.0};

    int m_targetIndex{-1};
    QString m_targetAppId;
    QString m_targetDisplayName;
    QString m_targetExec;
    QString m_targetIconType;
    int m_targetKind{0};
    bool m_isRunning{false};
    bool m_isActive{false};
    bool m_isPinned{false};
    int m_toplevelCount{0};
    QVariantList m_windowsList;
};

} // namespace tinexus::dock
