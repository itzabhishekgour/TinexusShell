#pragma once

#include <txui/widgets/Widget.hpp>

namespace txui {

class ScrollArea : public Widget {
private:
    double m_scroll_y{0.0};
    double m_scroll_x{0.0};

    double m_max_scroll_y{0.0};
    double m_max_scroll_x{0.0};
    
    bool m_allow_scroll_x{true};
    bool m_allow_scroll_y{true};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void layout_override(const Rect& frame) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    ScrollArea() noexcept = default;
    ~ScrollArea() override = default;

    void set_scroll_y(double y) noexcept;
    void set_scroll_x(double x) noexcept;

    void set_allow_scroll_x(bool allow) noexcept { m_allow_scroll_x = allow; mark_needs_measure(); }
    void set_allow_scroll_y(bool allow) noexcept { m_allow_scroll_y = allow; mark_needs_measure(); }

    [[nodiscard]] double scroll_y() const noexcept { return m_scroll_y; }
    [[nodiscard]] double scroll_x() const noexcept { return m_scroll_x; }
    [[nodiscard]] bool allow_scroll_x() const noexcept { return m_allow_scroll_x; }
    [[nodiscard]] bool allow_scroll_y() const noexcept { return m_allow_scroll_y; }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
