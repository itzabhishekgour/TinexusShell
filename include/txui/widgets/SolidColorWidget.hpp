#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/graphics/Color.hpp>

namespace txui {

class SolidColorWidget final : public Widget {
private:
    Color m_color;

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        // Expand to fill available space by default
        return Size(
            constraints.is_bounded_width() ? constraints.max_width : 0.0,
            constraints.is_bounded_height() ? constraints.max_height : 0.0
        );
    }

    void paint_override(Painter& painter) const noexcept override {
        painter.fill_rect(frame(), m_color);
    }

public:
    explicit SolidColorWidget(const Color& color) noexcept : m_color(color) {}
    ~SolidColorWidget() override = default;

    void set_color(const Color& color) noexcept {
        if (!(m_color == color)) {
            m_color = color;
            mark_needs_paint();
        }
    }
};

} // namespace txui
