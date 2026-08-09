#include <txui/widgets/Icon.hpp>

namespace txui {

Icon::Icon(IconType type, double size) noexcept : m_type(type), m_size(size) {}

void Icon::set_type(IconType type) noexcept {
    if (m_type != type) {
        m_type = type;
        mark_needs_paint();
    }
}

void Icon::set_size(double size) noexcept {
    if (m_size != size) {
        m_size = size;
        mark_needs_measure();
    }
}

Size Icon::measure_override(const Constraints& constraints) noexcept {
    return constraints.constrain(Size(m_size, m_size));
}

void Icon::paint_override(Painter& painter) const noexcept {
    Rect f = frame();
    
    // MVP Placeholder Shapes
    switch (m_type) {
        case IconType::Folder:
            // Blue rounded rect for folder
            painter.fill_rounded_rect(f, m_size * 0.2, Color(50, 150, 250, 255));
            break;
        case IconType::Executable:
            // Green diamond/circle for executable
            painter.fill_circle(Point(f.left() + f.width()/2, f.top() + f.height()/2), m_size * 0.5, Color(50, 200, 100, 255));
            break;
        case IconType::Archive:
            // Orange box for archive
            painter.fill_rect(f, Color(220, 150, 50, 255));
            break;
        case IconType::Image:
            // Purple rounded rect for image
            painter.fill_rounded_rect(f, m_size * 0.1, Color(200, 50, 200, 255));
            break;
        case IconType::File:
        default:
            // White document rectangle
            painter.fill_rect(Rect(f.left() + m_size*0.1, f.top(), m_size*0.8, m_size), Color(200, 200, 200, 255));
            break;
    }
}

} // namespace txui
