#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/FontMetrics.hpp>
#include <string>

namespace txui {

class Badge : public Widget {
private:
    std::string m_text;
    Color m_background_color{40, 44, 56, 200};
    Color m_text_color{200, 205, 220, 240};
    double m_font_size{11.0};
    double m_padding_x{8.0};
    double m_padding_y{3.0};
    double m_corner_radius{4.0};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    Badge() noexcept = default;
    explicit Badge(std::string text,
                   Color bg = Color(40, 44, 56, 200),
                   Color fg = Color(200, 205, 220, 240),
                   double font_size = 11.0) noexcept;
    ~Badge() override = default;

    void set_text(std::string text) noexcept;
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

    void set_background_color(Color color) noexcept;
    [[nodiscard]] Color background_color() const noexcept { return m_background_color; }

    void set_text_color(Color color) noexcept;
    [[nodiscard]] Color text_color() const noexcept { return m_text_color; }

    void set_font_size(double size) noexcept;
    [[nodiscard]] double font_size() const noexcept { return m_font_size; }

    void set_corner_radius(double radius) noexcept;
    [[nodiscard]] double corner_radius() const noexcept { return m_corner_radius; }

    void set_padding(double px, double py) noexcept;

    /// Canonical rendering helper for non-widget layouts or inline drawing.
    /// Returns the exact bounding Rect of the rendered badge.
    static Rect render(Painter& painter, const Point& origin, const std::string& text,
                       const Color& bg, const Color& fg,
                       double font_size = 11.0, double radius = 4.0,
                       double pad_x = 8.0, double pad_y = 3.0) noexcept;
};

} // namespace txui
