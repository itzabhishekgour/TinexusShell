#include "TerminalCanvas.hpp"
#include <QPainter>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

namespace tinexus::terminal {

TerminalCanvas::TerminalCanvas(QQuickItem* parent)
    : QQuickPaintedItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setActiveFocusOnTab(true);

    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(10);
    m_font.setStyleHint(QFont::Monospace);
    updateFontMetrics();
}

void TerminalCanvas::setBridge(TerminalBridge* bridge) {
    if (m_bridge == bridge) return;

    if (m_bridge) {
        disconnect(m_bridge, nullptr, this, nullptr);
    }

    m_bridge = bridge;

    if (m_bridge) {
        connect(m_bridge, &TerminalBridge::screenDamaged, this, [this]() { update(); });
        connect(m_bridge, &TerminalBridge::cursorMoved, this, [this](int, int) { update(); });
        connect(m_bridge, &TerminalBridge::blinkStateChanged, this, [this](bool) { update(); });
        connect(m_bridge, &TerminalBridge::scrollChanged, this, [this](int) { update(); });

        if (width() > 0 && height() > 0) {
            int cols = std::max(20, static_cast<int>(width() / m_cell_width));
            int rows = std::max(5, static_cast<int>(height() / m_cell_height));
            m_bridge->resizeTerminal(rows, cols);
        }
    }

    emit bridgeChanged();
    update();
}

void TerminalCanvas::updateFontMetrics() {
    QFontMetricsF fm(m_font);
    m_cell_width = std::max(6.0, fm.horizontalAdvance(QLatin1Char('M')));
    m_cell_height = std::max(12.0, fm.height());
    m_font_ascent = fm.ascent();
}

void TerminalCanvas::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.width() <= 0 || newGeometry.height() <= 0) return;

    int cols = std::max(20, static_cast<int>(newGeometry.width() / m_cell_width));
    int rows = std::max(5, static_cast<int>(newGeometry.height() / m_cell_height));

    if (m_bridge && (cols != m_bridge->cols() || rows != m_bridge->rows())) {
        m_bridge->resizeTerminal(rows, cols);
    }
}

void TerminalCanvas::normalizeSelection(int& s_r, int& s_c, int& e_r, int& e_c) const noexcept {
    s_r = m_sel_start_row;
    s_c = m_sel_start_col;
    e_r = m_sel_end_row;
    e_c = m_sel_end_col;

    if (s_r > e_r || (s_r == e_r && s_c > e_c)) {
        std::swap(s_r, e_r);
        std::swap(s_c, e_c);
    }
}

bool TerminalCanvas::isCellSelected(int r, int c) const noexcept {
    if (m_sel_start_row < 0 || m_sel_end_row < 0) return false;

    int s_r, s_c, e_r, e_c;
    normalizeSelection(s_r, s_c, e_r, e_c);

    if (r < s_r || r > e_r) return false;
    if (r == s_r && r == e_r) {
        return c >= s_c && c <= e_c;
    }
    if (r == s_r) return c >= s_c;
    if (r == e_r) return c <= e_c;
    return true;
}

void TerminalCanvas::paint(QPainter* painter) {
    if (!painter) return;

    // Base background
    painter->fillRect(boundingRect(), QColor(24, 24, 28));

    if (!m_bridge || !m_bridge->emulator()) return;

    TerminalEmulator* emu = m_bridge->emulator();
    const int rows = m_bridge->rows();
    const int cols = m_bridge->cols();

    painter->setRenderHint(QPainter::TextAntialiasing, true);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto cell = emu->get_cell(r, c);
            QRectF cellRect(c * m_cell_width, r * m_cell_height, m_cell_width, m_cell_height);

            QColor fg(cell.attrs.fg_red, cell.attrs.fg_green, cell.attrs.fg_blue);
            QColor bg(cell.attrs.bg_red, cell.attrs.bg_green, cell.attrs.bg_blue);

            if (cell.attrs.fg_red == 0 && cell.attrs.fg_green == 0 && cell.attrs.fg_blue == 0) {
                fg = QColor(215, 215, 222);
            }
            if (cell.attrs.bg_red == 0 && cell.attrs.bg_green == 0 && cell.attrs.bg_blue == 0) {
                bg = QColor(24, 24, 28);
            }

            if (cell.attrs.reverse) {
                std::swap(fg, bg);
            }

            // Cell custom background
            if (bg != QColor(24, 24, 28)) {
                painter->fillRect(cellRect, bg);
            }

            // Selection highlight
            if (isCellSelected(r, c)) {
                painter->fillRect(cellRect, QColor(100, 180, 255, 80));
            }

            // Text glyph
            if (cell.codepoint != 0 && cell.codepoint != ' ') {
                QFont f = m_font;
                if (cell.attrs.bold) f.setBold(true);
                if (cell.attrs.italic) f.setItalic(true);
                if (cell.attrs.underline) f.setUnderline(true);

                painter->setFont(f);
                painter->setPen(fg);

                const char32_t cp = static_cast<char32_t>(cell.codepoint);
                QString glyph = QString::fromUcs4(&cp, 1);
                painter->drawText(QPointF(cellRect.left(), cellRect.top() + m_font_ascent), glyph);
            }
        }
    }

    // Cursor
    if (m_bridge->scrollOffset() == 0 && m_bridge->blinkState()) {
        const int cr = m_bridge->cursorRow();
        const int cc = m_bridge->cursorCol();
        if (cr >= 0 && cr < rows && cc >= 0 && cc < cols) {
            QRectF curRect(cc * m_cell_width, cr * m_cell_height, m_cell_width, m_cell_height);
            painter->fillRect(curRect, QColor(130, 210, 255, 200));

            auto curCell = emu->get_cell(cr, cc);
            if (curCell.codepoint != 0 && curCell.codepoint != ' ') {
                painter->setFont(m_font);
                painter->setPen(QColor(15, 15, 20));
                const char32_t ccp = static_cast<char32_t>(curCell.codepoint);
                QString glyph = QString::fromUcs4(&ccp, 1);
                painter->drawText(QPointF(curRect.left(), curRect.top() + m_font_ascent), glyph);
            }
        }
    }
}

void TerminalCanvas::keyPressEvent(QKeyEvent* event) {
    if (!m_bridge) {
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }

    // Shortcut: Ctrl+Shift+C -> Copy
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) &&
        event->key() == Qt::Key_C) {
        int s_r, s_c, e_r, e_c;
        normalizeSelection(s_r, s_c, e_r, e_c);
        if (s_r >= 0) {
            QString sel = m_bridge->getSelectedText(s_r, s_c, e_r, e_c);
            m_bridge->copyToClipboard(sel);
        }
        event->accept();
        return;
    }

    // Shortcut: Ctrl+Shift+V -> Paste
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier) &&
        event->key() == Qt::Key_V) {
        m_bridge->pasteFromClipboard();
        event->accept();
        return;
    }

    m_bridge->sendKey(event->key(), event->modifiers(), event->text());
    event->accept();
}

void TerminalCanvas::mousePressEvent(QMouseEvent* event) {
    forceActiveFocus();
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_sel_start_row = std::clamp(static_cast<int>(event->position().y() / m_cell_height),
                                     0, m_bridge ? m_bridge->rows() - 1 : 0);
        m_sel_start_col = std::clamp(static_cast<int>(event->position().x() / m_cell_width),
                                     0, m_bridge ? m_bridge->cols() - 1 : 0);
        m_sel_end_row = m_sel_start_row;
        m_sel_end_col = m_sel_start_col;
        update();
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        if (m_bridge) {
            m_bridge->pasteFromClipboard();
        }
        event->accept();
    } else {
        QQuickPaintedItem::mousePressEvent(event);
    }
}

void TerminalCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_selecting && m_bridge) {
        m_sel_end_row = std::clamp(static_cast<int>(event->position().y() / m_cell_height),
                                   0, m_bridge->rows() - 1);
        m_sel_end_col = std::clamp(static_cast<int>(event->position().x() / m_cell_width),
                                   0, m_bridge->cols() - 1);
        update();
        event->accept();
    } else {
        QQuickPaintedItem::mouseMoveEvent(event);
    }
}

void TerminalCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (m_selecting) {
        m_selecting = false;
        update();
        event->accept();
    } else {
        QQuickPaintedItem::mouseReleaseEvent(event);
    }
}

void TerminalCanvas::wheelEvent(QWheelEvent* event) {
    if (m_bridge) {
        int dy = event->angleDelta().y();
        if (dy > 0) {
            m_bridge->scrollUp(3);
        } else if (dy < 0) {
            m_bridge->scrollDown(3);
        }
        event->accept();
    } else {
        QQuickPaintedItem::wheelEvent(event);
    }
}

} // namespace tinexus::terminal
