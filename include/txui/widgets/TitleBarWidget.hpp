#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <txui/graphics/Color.hpp>
#include <string>
#include <functional>

namespace txui {

class TitleBarWidget : public Widget {
public:
    using Callback = std::function<void()>;
    using MoveCallback = std::function<void(uint32_t)>;

private:
    std::string m_title;
    Callback m_on_close;
    Callback m_on_minimize;
    Callback m_on_maximize;
    MoveCallback m_on_move;

    // UI state
    int m_hovered_button{-1}; // 0: close, 1: minimize, 2: maximize, -1: none
    bool m_is_maximized{false}; // toggled on each maximize click

    // Layout geometry — Apple HIG proportions
    constexpr static double BUTTON_RADIUS  = 6.0;   // 12px diameter
    constexpr static double BUTTON_SPACING = 18.0;  // tighter Apple-style gap
    constexpr static double LEFT_PADDING   = 16.0;

    [[nodiscard]] Rect button_rect(int index) const noexcept;

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    explicit TitleBarWidget(
        std::string_view title,
        std::function<void()> on_close,
        std::function<void()> on_minimize = {},
        std::function<void()> on_maximize = {},
        MoveCallback on_move = {}
    ) noexcept;

    ~TitleBarWidget() override = default;

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
