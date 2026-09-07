#pragma once

#include <txui/widgets/Widget.hpp>
#include <vector>
#include <functional>

namespace txui {

class ColorPicker : public Widget {
private:
    std::vector<Color> m_colors;
    size_t m_selected_index{0};
    int m_hovered_index{-1};
    bool m_pressed{false};

    double m_swatch_radius{12.0};
    double m_spacing{18.0};

    std::function<void(size_t, Color)> m_on_color_selected;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    ColorPicker() noexcept = default;
    explicit ColorPicker(std::vector<Color> colors, size_t selected_index = 0,
                         std::function<void(size_t, Color)> on_selected = nullptr) noexcept;
    ~ColorPicker() override = default;

    void set_colors(std::vector<Color> colors) noexcept;
    [[nodiscard]] const std::vector<Color>& colors() const noexcept { return m_colors; }

    void set_selected_index(size_t index) noexcept;
    [[nodiscard]] size_t selected_index() const noexcept { return m_selected_index; }
    [[nodiscard]] Color selected_color() const noexcept;

    void set_swatch_radius(double radius) noexcept;
    [[nodiscard]] double swatch_radius() const noexcept { return m_swatch_radius; }

    void set_spacing(double spacing) noexcept;
    [[nodiscard]] double spacing() const noexcept { return m_spacing; }

    void set_on_color_selected(std::function<void(size_t, Color)> callback) noexcept {
        m_on_color_selected = std::move(callback);
    }

    bool handle_event(const Event& event) noexcept override;

private:
    [[nodiscard]] int hit_test_index(double x, double y) const noexcept;
};

} // namespace txui
