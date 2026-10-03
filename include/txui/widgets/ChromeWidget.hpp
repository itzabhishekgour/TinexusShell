#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/TitleBarWidget.hpp>
#include <txui/core/Ref.hpp>
#include <functional>

namespace txui {

// ChromeWidget wraps any content widget with a macOS-style title bar
// (traffic light close/minimize/maximize buttons) and rounded-corner frame.
class ChromeWidget : public Widget {
public:
    using ResizeCallback = std::function<void(uint32_t edges, uint32_t serial)>;

private:
    Ref<FlexLayout>      m_layout;
    Ref<TitleBarWidget>  m_title_bar;
    Ref<FlexItem>        m_content_item;
    ResizeCallback       m_on_resize;
    bool                 m_is_maximized{false};
    uint32_t             m_hovered_edge{0};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void layout_override(const Rect& frame) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    explicit ChromeWidget(
        std::string_view title,
        Ref<Widget> content,
        std::function<void()> on_close,
        std::function<void()> on_minimize = {},
        std::function<void()> on_maximize = {},
        std::function<void(uint32_t)> on_move = {},
        ResizeCallback on_resize = {}
    ) noexcept;

    ~ChromeWidget() override = default;

    void set_maximized(bool maximized) noexcept {
        if (m_is_maximized != maximized) {
            m_is_maximized = maximized;
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool is_maximized() const noexcept { return m_is_maximized; }

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
