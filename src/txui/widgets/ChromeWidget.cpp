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

constexpr double BORDER_HIT_THICKNESS = 8.0;

// Wayland resize edge constants (matching xdg_toplevel and wlr_edges)
constexpr uint32_t EDGE_TOP    = 1;
constexpr uint32_t EDGE_BOTTOM = 2;
constexpr uint32_t EDGE_LEFT   = 4;
constexpr uint32_t EDGE_RIGHT  = 8;

ChromeWidget::ChromeWidget(
    std::string_view title,
    Ref<Widget> content,
    std::function<void()> on_close,
    std::function<void()> on_minimize,
    std::function<void()> on_maximize,
    std::function<void(uint32_t)> on_move,
    ResizeCallback on_resize
) noexcept
    : m_on_resize(std::move(on_resize)) {
    m_title_bar = make_ref<TitleBarWidget>(
        title,
        std::move(on_close),
        std::move(on_minimize),
        std::move(on_maximize),
        std::move(on_move)
    );

    // Wrap content in FlexItem (flex=1, expanded) so it fills the remaining height
    m_content_item = make_ref<FlexItem>(1, true, std::move(content));

    // Column layout: TitleBar on top, content below
    m_layout = make_ref<Column>();
    m_layout->set_cross_axis_alignment(CrossAxisAlignment::Stretch);
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

    if (m_is_maximized) {
        // ── Maximized / Tiled Mode ─────────────────────────────────────────────
        // Square corners (radius = 0), zero drop shadow spread, filling 100%
        // of the allocated work area without any unwanted gaps/margins.
        painter.fill_rect(
            Rect(0.0, 0.0, f.width(), f.height()),
            Color(10, 10, 14, 255)
        );
    } else {
        // ── Normal Floating Mode ───────────────────────────────────────────────
        // Drop shadow (client-side, painted before the window background)
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

        // Window background with rounded corners
        painter.fill_rounded_rect(
            Rect(0.0, 0.0, f.width(), f.height()),
            WINDOW_RADIUS,
            Color(10, 10, 14, 255)
        );
    }

    // Paint children (FlexLayout → TitleBar + content)
    paint_children(painter);
}

bool ChromeWidget::handle_event(const Event& event) noexcept {
    if (m_is_maximized) {
        // Maximized windows do not initiate edge resize
        return Widget::handle_event(event);
    }

    if (event.type == EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;
        double w = frame().width();
        double h = frame().height();

        m_hovered_edge = 0;
        if (px <= BORDER_HIT_THICKNESS) m_hovered_edge |= EDGE_LEFT;
        if (px >= w - BORDER_HIT_THICKNESS) m_hovered_edge |= EDGE_RIGHT;
        if (py <= BORDER_HIT_THICKNESS) m_hovered_edge |= EDGE_TOP;
        if (py >= h - BORDER_HIT_THICKNESS) m_hovered_edge |= EDGE_BOTTOM;
    } else if (event.type == EventType::PointerButtonPress) {
        if (m_hovered_edge != 0 && m_on_resize && event.pointer.button == MouseButton::Left) {
            m_on_resize(m_hovered_edge, event.pointer.serial);
            return true;
        }
    }

    return Widget::handle_event(event);
}

} // namespace txui
