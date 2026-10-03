// ============================================================================
// StacksPopup.hpp — Layer-Shell Overlay Surface for Stacks Popover (Slice 7)
// Ref: Architecture Blueprint §8, docs/05_UI_UX_GUIDELINES.md
// Hosts StacksPopover.qml on LayerOverlay with LiquidGlass design & grid view.
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlApplicationEngine>

namespace tinexus::dock {

class StacksModel;

class StacksPopup : public QObject {
    Q_OBJECT

    Q_PROPERTY(double popupX READ popupX NOTIFY geometryChanged)
    Q_PROPERTY(double popupY READ popupY NOTIFY geometryChanged)
    Q_PROPERTY(double popupWidth READ popupWidth CONSTANT)
    Q_PROPERTY(double popupHeight READ popupHeight CONSTANT)
    Q_PROPERTY(double screenWidth READ screenWidth NOTIFY geometryChanged)
    Q_PROPERTY(double screenHeight READ screenHeight NOTIFY geometryChanged)
    Q_PROPERTY(double targetCenterX READ targetCenterX NOTIFY targetChanged)
    Q_PROPERTY(QString folderTitle READ folderTitle NOTIFY targetChanged)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openStateChanged)

public:
    static constexpr double DEFAULT_POPUP_WIDTH  = 440.0;
    static constexpr double DEFAULT_POPUP_HEIGHT = 460.0;

    explicit StacksPopup(StacksModel* model = nullptr, QObject* parent = nullptr);
    ~StacksPopup() override;

    bool init(QQmlApplicationEngine* engine);

    [[nodiscard]] double popupX() const { return m_popupX; }
    [[nodiscard]] double popupY() const { return m_popupY; }
    [[nodiscard]] double popupWidth() const { return DEFAULT_POPUP_WIDTH; }
    [[nodiscard]] double popupHeight() const { return DEFAULT_POPUP_HEIGHT; }
    [[nodiscard]] double screenWidth() const { return m_screenWidth; }
    [[nodiscard]] double screenHeight() const { return m_screenHeight; }
    [[nodiscard]] double targetCenterX() const { return m_targetCenterX; }
    [[nodiscard]] QString folderTitle() const { return m_folderTitle; }
    [[nodiscard]] bool isOpen() const { return m_isOpen; }
    [[nodiscard]] QQuickWindow* window() const { return m_window; }
    [[nodiscard]] StacksModel* model() const { return m_model; }

    void setModel(StacksModel* model) { m_model = model; }
    void updateScreenGeometry(double width, double height);

    // QML Invokables
    Q_INVOKABLE void showPopup(double screenCenterX, double targetTopY, const QString& path = QString(), const QString& title = QString());
    Q_INVOKABLE void hidePopup();
    Q_INVOKABLE void togglePopup(double screenCenterX, double targetTopY, const QString& path = QString(), const QString& title = QString());

signals:
    void geometryChanged();
    void targetChanged();
    void openStateChanged(bool open);

private:
    void setupLayerShell();

    StacksModel*  m_model{nullptr};
    QQuickWindow* m_window{nullptr};

    double  m_popupX{0.0};
    double  m_popupY{0.0};
    double  m_targetCenterX{0.0};
    double  m_screenWidth{1920.0};
    double  m_screenHeight{1080.0};
    double  m_dpr{1.0};
    QString m_folderTitle;
    bool    m_isOpen{false};
};

} // namespace tinexus::dock
