#pragma once

#include <txui/widgets/Widget.hpp>
#include <functional>

namespace txui {

class Slider : public Widget {
private:
    double m_min{0.0};
    double m_max{1.0};
    double m_value{0.0};

    bool m_dragging{false};
    bool m_hovered{false};
    bool m_enabled{true};

    Color m_active_color{0, 195, 255, 255};
    Color m_inactive_color{48, 52, 66, 240};

    std::function<void(double)> m_on_value_changed;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    Slider() noexcept = default;
    explicit Slider(double min_val, double max_val, double initial_val = 0.0) noexcept;
    ~Slider() override = default;

    void set_range(double min_val, double max_val) noexcept;
    [[nodiscard]] double min_value() const noexcept { return m_min; }
    [[nodiscard]] double max_value() const noexcept { return m_max; }

    void set_value(double val) noexcept;
    [[nodiscard]] double value() const noexcept { return m_value; }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    void set_active_color(Color color) noexcept;
    [[nodiscard]] Color active_color() const noexcept { return m_active_color; }

    void set_inactive_color(Color color) noexcept;
    [[nodiscard]] Color inactive_color() const noexcept { return m_inactive_color; }

    void set_on_value_changed(std::function<void(double)> callback) noexcept { m_on_value_changed = std::move(callback); }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
