#include "txui/widgets/LineGraphWidget.hpp"
#include <algorithm>
#include <cmath>

namespace txui {

LineGraphWidget::LineGraphWidget(std::string title) noexcept
    : m_title(std::move(title)) {
}

void LineGraphWidget::set_title(std::string title) noexcept {
    m_title = std::move(title);
    mark_needs_paint();
}

void LineGraphWidget::set_current_value_string(std::string val_str) noexcept {
    m_current_value_str = std::move(val_str);
    mark_needs_paint();
}

void LineGraphWidget::add_series(const std::string& name, const Color& stroke_color,
                                const Color& fill_start, const Color& fill_end) noexcept {
    Series s;
    s.name = name;
    s.stroke_color = stroke_color;
    s.fill_color_start = (fill_start.a() > 0) ? fill_start : Color(stroke_color.r(), stroke_color.g(), stroke_color.b(), 60);
    s.fill_color_end = (fill_end.a() > 0) ? fill_end : Color(stroke_color.r(), stroke_color.g(), stroke_color.b(), 5);
    m_series.push_back(s);
}

void LineGraphWidget::push_value(size_t series_idx, float value) noexcept {
    if (series_idx >= m_series.size()) return;
    auto& s = m_series[series_idx];
    s.values.push_back(value);
    while (s.values.size() > m_capacity) {
        s.values.pop_front();
    }
    mark_needs_paint();
}

void LineGraphWidget::set_series_values(size_t series_idx, const std::vector<float>& values) noexcept {
    if (series_idx >= m_series.size()) return;
    auto& s = m_series[series_idx];
    s.values.clear();
    for (float v : values) {
        s.values.push_back(v);
    }
    while (s.values.size() > m_capacity) {
        s.values.pop_front();
    }
    mark_needs_paint();
}

void LineGraphWidget::set_series_range(size_t series_idx, float min_val, float max_val) noexcept {
    if (series_idx >= m_series.size()) return;
    m_series[series_idx].min_value = min_val;
    m_series[series_idx].max_value = max_val;
    mark_needs_paint();
}

Size LineGraphWidget::measure_override(const Constraints& constraints) noexcept {
    double w = std::clamp(280.0, constraints.min_width, constraints.max_width);
    double h = std::clamp(150.0, constraints.min_height, constraints.max_height);
    return Size(w, h);
}

void LineGraphWidget::layout_override(const Rect& /*frame*/) noexcept {
    // Childless self-rendering widget
}

void LineGraphWidget::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    if (f.width() <= 10.0 || f.height() <= 10.0) return;

    // Card background & subtle 1px border
    painter.fill_rounded_rect(f, 8.0, Color(22, 22, 30, 240));
    painter.fill_rounded_rect(Rect(f.x(), f.y(), f.width(), 1.0), 0.0, Color(255, 255, 255, 18));

    // Title & current value string
    double header_y = f.y() + 18.0;
    if (!m_title.empty()) {
        painter.draw_text(Point(static_cast<Coordinate>(f.x() + 14.0), static_cast<Coordinate>(header_y)),
                          m_title, Color(230, 230, 240, 255), 12.5, true);
    }
    if (!m_current_value_str.empty()) {
        auto ext = painter.measure_text(m_current_value_str, 12.0, true);
        double val_x = f.x() + f.width() - ext.width - 14.0;
        painter.draw_text(Point(static_cast<Coordinate>(val_x), static_cast<Coordinate>(header_y)),
                          m_current_value_str, Color(160, 180, 210, 255), 12.0, true);
    }

    // Graph plotting area
    double gx = f.x() + 14.0;
    double gy = f.y() + 34.0;
    double gw = f.width() - 28.0;
    double gh = f.height() - 46.0;
    if (gw <= 20.0 || gh <= 20.0) return;

    // Background grid lines (100%, 66%, 33%, 0%)
    if (m_show_grid) {
        Color grid_col(255, 255, 255, 12);
        // Top 100% line
        painter.draw_line(Point(static_cast<Coordinate>(gx), static_cast<Coordinate>(gy)),
                          Point(static_cast<Coordinate>(gx + gw), static_cast<Coordinate>(gy)), 1.0, grid_col);
        // 66% line
        painter.draw_line(Point(static_cast<Coordinate>(gx), static_cast<Coordinate>(gy + gh * 0.33)),
                          Point(static_cast<Coordinate>(gx + gw), static_cast<Coordinate>(gy + gh * 0.33)), 1.0, grid_col);
        // 33% line
        painter.draw_line(Point(static_cast<Coordinate>(gx), static_cast<Coordinate>(gy + gh * 0.66)),
                          Point(static_cast<Coordinate>(gx + gw), static_cast<Coordinate>(gy + gh * 0.66)), 1.0, grid_col);
        // Bottom baseline
        painter.draw_line(Point(static_cast<Coordinate>(gx), static_cast<Coordinate>(gy + gh)),
                          Point(static_cast<Coordinate>(gx + gw), static_cast<Coordinate>(gy + gh)), 1.0, Color(255, 255, 255, 24));
    }

    // Plot each series
    for (const auto& s : m_series) {
        // Continuous subtle series baseline across the full width so it reads as "live & monitoring"
        Color idle_baseline(s.stroke_color.r(), s.stroke_color.g(), s.stroke_color.b(), 65);
        painter.draw_line(Point(static_cast<Coordinate>(gx), static_cast<Coordinate>(gy + gh)),
                          Point(static_cast<Coordinate>(gx + gw), static_cast<Coordinate>(gy + gh)), 1.0, idle_baseline);

        if (s.values.size() < 2) continue;

        float range = (s.max_value > s.min_value) ? (s.max_value - s.min_value) : 1.0f;
        size_t count = s.values.size();
        double step_x = gw / static_cast<double>(m_capacity > 1 ? (m_capacity - 1) : 1);
        double start_x = gx + gw - static_cast<double>(count - 1) * step_x;

        auto calc_pt = [&](size_t idx) -> Point {
            float val = std::clamp(s.values[idx], s.min_value, s.max_value);
            float norm = (val - s.min_value) / range;
            double px = start_x + static_cast<double>(idx) * step_x;
            double py = gy + gh - static_cast<double>(norm) * gh;
            return Point(static_cast<Coordinate>(px), static_cast<Coordinate>(py));
        };

        // Fill area under line with soft gradient bars
        if (m_fill_area) {
            for (size_t i = 0; i < count - 1; ++i) {
                Point p1 = calc_pt(i);
                Point p2 = calc_pt(i + 1);
                double bar_x = static_cast<double>(p1.x);
                double bar_w = std::max(1.0, static_cast<double>(p2.x - p1.x));
                double avg_y = static_cast<double>(p1.y + p2.y) * 0.5;
                double bar_h = (gy + gh) - avg_y;
                if (bar_h > 0.8) {
                    painter.fill_gradient_rect(Rect(bar_x, avg_y, bar_w, bar_h),
                                               s.fill_color_start, s.fill_color_end, false);
                }
            }
        }

        // Draw polyline segments
        for (size_t i = 0; i < count - 1; ++i) {
            Point p1 = calc_pt(i);
            Point p2 = calc_pt(i + 1);
            painter.draw_line(p1, p2, 2.0, s.stroke_color);
        }

        // Endpoint indicator: only render pulsing dot if latest value is active (> 0.05 of range)
        float latest_val = s.values.back();
        if (latest_val > (s.min_value + range * 0.02f)) {
            Point latest = calc_pt(count - 1);
            painter.fill_circle(latest, 4.0, s.stroke_color);
            painter.fill_circle(latest, 2.0, Color(255, 255, 255, 255));
        }
    }
}

} // namespace txui
