#pragma once

#include "widget.hpp"
#include "color.hpp"

namespace ui {

class PanelWidget : public Widget {
public:
    void layout(int width, int height) override;
    void draw(Painter& painter) override;

private:
    int m_width{0};
    int m_height{0};
    // Tinexus dark panel — deep navy/charcoal with full opacity
    Color m_bg_color{15, 18, 30, 245}; // R=15 G=18 B=30 A=245 (dark navy, near-opaque)
};

} // namespace ui
