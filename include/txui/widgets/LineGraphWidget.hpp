#pragma once

#include <txui/widgets/Widget.hpp>
#include <deque>
#include <string>
#include <vector>

namespace txui {

class LineGraphWidget : public Widget {
public:
    struct Series {
        std::string name;
        Color stroke_color;
        Color fill_color_start;
        Color fill_color_end;
        std::deque<float> values;
        float max_value{100.0f};
        float min_value{0.0f};
    };

private:
    std::string m_title;
    std::string m_current_value_str;
    std::vector<Series> m_series;
    size_t m_capacity{60};
    bool m_show_grid{true};
    bool m_fill_area{true};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void layout_override(const Rect& frame) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    explicit LineGraphWidget(std::string title = "") noexcept;
    ~LineGraphWidget() override = default;

    void set_title(std::string title) noexcept;
    [[nodiscard]] const std::string& title() const noexcept { return m_title; }

    void set_current_value_string(std::string val_str) noexcept;

    void add_series(const std::string& name, const Color& stroke_color,
                    const Color& fill_start = Color(0, 0, 0, 0),
                    const Color& fill_end = Color(0, 0, 0, 0)) noexcept;

    void push_value(size_t series_idx, float value) noexcept;
    void set_series_values(size_t series_idx, const std::vector<float>& values) noexcept;
    void set_series_range(size_t series_idx, float min_val, float max_val) noexcept;

    void set_capacity(size_t cap) noexcept { m_capacity = cap; }
    void set_show_grid(bool show) noexcept { m_show_grid = show; }
    void set_fill_area(bool fill) noexcept { m_fill_area = fill; }
};

} // namespace txui
