#pragma once

#include <QQuickPaintedItem>
#include <QFont>
#include <QFontMetricsF>
#include <QColor>
#include "TerminalBridge.hpp"

namespace tinexus::terminal {

class TerminalCanvas : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(TerminalBridge* bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)

public:
    explicit TerminalCanvas(QQuickItem* parent = nullptr);
    ~TerminalCanvas() override = default;

    TerminalBridge* bridge() const noexcept { return m_bridge; }
    void setBridge(TerminalBridge* bridge);

    void paint(QPainter* painter) override;

signals:
    void bridgeChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateFontMetrics();
    bool isCellSelected(int r, int c) const noexcept;
    void normalizeSelection(int& s_r, int& s_c, int& e_r, int& e_c) const noexcept;

    TerminalBridge* m_bridge{nullptr};
    QFont           m_font;
    qreal           m_cell_width{9.0};
    qreal           m_cell_height{18.0};
    qreal           m_font_ascent{14.0};

    bool m_selecting{false};
    int  m_sel_start_row{-1};
    int  m_sel_start_col{-1};
    int  m_sel_end_row{-1};
    int  m_sel_end_col{-1};
};

} // namespace tinexus::terminal
