#include "panel_widget.hpp"

namespace ui {

void PanelWidget::layout(int width, int height) {
    m_width = width;
    m_height = height;
}

void PanelWidget::draw(Painter& painter) {
    painter.clear(m_bg_color);
}

} // namespace ui
