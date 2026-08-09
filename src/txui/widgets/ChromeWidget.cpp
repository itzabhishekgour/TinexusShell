#include <txui/widgets/ChromeWidget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/math/Rect.hpp>
#include <txui/graphics/Color.hpp>

namespace txui {

// Window corner radius — matches macOS ~11px rounding
constexpr double WINDOW_RADIUS = 11.0;

// Drop shadow — semi-transparent black, painted behind the window rect
// at a small offset/spread to create depth against the wallpaper
constexpr Color SHADOW_COLOR{0, 0, 0, 55};

ChromeWidget::ChromeWidget(
    std::string_view title,
    Ref<Widget> content,
    std::function<void()> on_close,
    std::function<void()> on_minimize,
    std::function<void()> on_maximize
) noexcept {
    m_title_bar = make_ref<TitleBarWidget>(
        title,
        std::move(on_close),
        std::move(on_minimize),
        std::move(on_maximize)
    );

    // Wrap content in FlexItem (flex=1, expanded) so it fills the remaining height
    m_content_item = make_ref<FlexItem>(1, true, std::move(content));

    // Column layout: TitleBar on top, content below
    m_layout = make_ref<Column>();
    m_layout->add_child(m_title_bar);
    m_layout->add_child(m_content_item);

    add_child(m_layout);
}

Size ChromeWidget::measure_override(const Constraints& constraints) noexcept {
    m_layout->measure(constraints);
    return m_layout->desired_size();
}

void ChromeWidget::layout_override(const Rect& frame) noexcept {
    m_layout->layout(frame);
}

void ChromeWidget::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();

    // ── Drop shadow (client-side, painted before the window background) ──────
    // A larger, offset rounded rect with low opacity simulates a soft shadow.
    // This composites against the wallpaper behind our xdg_toplevel buffer.
    const double shadow_offset = 3.0;
    const double shadow_spread = 6.0;
    painter.fill_rounded_rect(
        Rect(f.x() - shadow_spread * 0.5 + shadow_offset,
             f.y() - shadow_spread * 0.5 + shadow_offset,
             f.width()  + shadow_spread,
             f.height() + shadow_spread),
        WINDOW_RADIUS + 2.0,
        SHADOW_COLOR
    );

    // ── Window background with rounded corners ───────────────────────────────
    // The rounded rect clips all content pixels to give macOS-style corners.
    painter.fill_rounded_rect(
        Rect(0.0, 0.0, f.width(), f.height()),
        WINDOW_RADIUS,
        Color(10, 10, 14, 255)
    );

    // Paint children (FlexLayout → TitleBar + content)
    paint_children(painter);
}

bool ChromeWidget::handle_event(const Event& event) noexcept {
    return Widget::handle_event(event);
}

} // namespace txui
