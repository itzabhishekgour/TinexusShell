#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/FontMetrics.hpp>
#include <string>
#include <vector>
#include <functional>

namespace txui {

class SegmentedControl : public Widget {
private:
    std::vector<std::string> m_segments;
    size_t m_selected_index{0};
    int m_hovered_index{-1};
    double m_font_size{13.0};
    double m_corner_radius{10.0};

    std::function<void(size_t)> m_on_segment_selected;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    SegmentedControl() noexcept = default;
    explicit SegmentedControl(std::vector<std::string> segments, size_t default_index = 0) noexcept;
    ~SegmentedControl() override = default;

    void set_segments(std::vector<std::string> segments) noexcept;
    [[nodiscard]] const std::vector<std::string>& segments() const noexcept { return m_segments; }

    void set_selected_index(size_t index) noexcept;
    [[nodiscard]] size_t selected_index() const noexcept { return m_selected_index; }

    void set_font_size(double size) noexcept;
    [[nodiscard]] double font_size() const noexcept { return m_font_size; }

    void set_corner_radius(double radius) noexcept;
    [[nodiscard]] double corner_radius() const noexcept { return m_corner_radius; }

    void set_on_segment_selected(std::function<void(size_t)> callback) noexcept {
        m_on_segment_selected = std::move(callback);
    }

    bool handle_event(const Event& event) noexcept override;

private:
    [[nodiscard]] Rect segment_rect(size_t index) const noexcept;
};

} // namespace txui
