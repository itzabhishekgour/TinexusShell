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
private:
    Ref<FlexLayout>      m_layout;
    Ref<TitleBarWidget>  m_title_bar;
    Ref<FlexItem>        m_content_item;

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
        std::function<void(uint32_t)> on_move = {}
    ) noexcept;

    ~ChromeWidget() override = default;

    bool handle_event(const Event& event) noexcept override;
};

} // namespace txui
