#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/graphics/Color.hpp>
#include <string>
#include <algorithm>

namespace txui {

class TextWidget final : public Widget {
private:
    std::string m_text;
    Color m_color;
    double m_scale{1.0};
    double m_offset_y{0.0};

protected:
    Size measure_override(const Constraints& constraints) noexcept override {
        double width = static_cast<double>(m_text.size() * 8) * m_scale;
        double height = 16.0 * m_scale;
        if (constraints.is_bounded_width()) {
            width = std::clamp(width, constraints.min_width, constraints.max_width);
        }
        if (constraints.is_bounded_height()) {
            height = std::clamp(height, constraints.min_height, constraints.max_height);
        }
        return Size(width, height);
    }

    void paint_override(Painter& painter) const noexcept override {
        Point pos = frame().origin;
        pos.y += m_offset_y;
        painter.draw_text(pos, m_text, m_color, m_scale);
    }

public:
    explicit TextWidget(std::string text = "", const Color& color = Color::white(), double scale = 1.0) noexcept
        : m_text(std::move(text)), m_color(color), m_scale(scale) {}
    ~TextWidget() override = default;

    void set_offset_y(double offset) noexcept {
        if (m_offset_y != offset) {
            m_offset_y = offset;
            mark_needs_paint();
        }
    }

    void set_text(const std::string& text) noexcept {
        if (m_text != text) {
            m_text = text;
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void set_color(const Color& color) noexcept {
        if (!(m_color == color)) {
            m_color = color;
            mark_needs_paint();
        }
    }

    void set_scale(double scale) noexcept {
        if (m_scale != scale) {
            m_scale = scale;
            mark_needs_layout();
            mark_needs_paint();
        }
    }
};

} // namespace txui
