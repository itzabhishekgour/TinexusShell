#include <txui/widgets/TitleBarWidget.hpp>
#include <txui/render/Command.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <cmath>

namespace txui {

// ── Apple-reference traffic light colors ─────────────────────────────────────
constexpr Color BTN_CLOSE_NORMAL {255,  95,  87, 255}; // #FF5F57
constexpr Color BTN_CLOSE_HOVER  {255, 125, 120, 255};
constexpr Color BTN_MIN_NORMAL   {255, 189,  46, 255}; // #FFBD2E
constexpr Color BTN_MIN_HOVER    {255, 205,  80, 255};
constexpr Color BTN_MAX_NORMAL   { 40, 200,  64, 255}; // #28C840
constexpr Color BTN_MAX_HOVER    { 65, 220,  90, 255};

// Title bar appearance
constexpr Color TITLEBAR_BG    { 22,  22,  26, 255}; // Very dark, slightly warm
constexpr Color TITLEBAR_BORDER{ 15,  15,  18, 255};
constexpr Color TITLE_TEXT     {170, 170, 175, 220}; // Muted gray, not white

// ─────────────────────────────────────────────────────────────────────────────

TitleBarWidget::TitleBarWidget(
    std::string_view title,
    std::function<void()> on_close,
    std::function<void()> on_minimize,
    std::function<void()> on_maximize,
    MoveCallback on_move
) noexcept
    : m_title(title),
      m_on_close(std::move(on_close)),
      m_on_minimize(std::move(on_minimize)),
      m_on_maximize(std::move(on_maximize)),
      m_on_move(std::move(on_move)) {}

Rect TitleBarWidget::button_rect(int index) const noexcept {
    double y = frame().y() + (frame().height() - (BUTTON_RADIUS * 2.0)) / 2.0;
    double x = frame().x() + LEFT_PADDING + static_cast<double>(index) * BUTTON_SPACING;
    return Rect(x, y, BUTTON_RADIUS * 2.0, BUTTON_RADIUS * 2.0);
}

Size TitleBarWidget::measure_override(const Constraints& constraints) noexcept {
    // Standard macOS titlebar height 42px, full width
    return Size(constraints.max_width, 42.0);
}

void TitleBarWidget::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();

    // Background
    painter.fill_rect(Rect(f.x(), f.y(), f.width(), f.height()), TITLEBAR_BG);

    // Bottom border — 1px separator line
    painter.fill_rect(Rect(f.x(), f.y() + f.height() - 1.0, f.width(), 1.0), TITLEBAR_BORDER);

    // Traffic light buttons
    const Color close_color = (m_hovered_button == 0) ? BTN_CLOSE_HOVER : BTN_CLOSE_NORMAL;
    const Color min_color   = (m_hovered_button == 1) ? BTN_MIN_HOVER   : BTN_MIN_NORMAL;
    const Color max_color   = (m_hovered_button == 2) ? BTN_MAX_HOVER   : BTN_MAX_NORMAL;

    auto draw_btn = [&](int idx, const Color& c) {
        Rect r = button_rect(idx);
        Point center{r.left() + BUTTON_RADIUS, r.top() + BUTTON_RADIUS};
        painter.fill_circle(center, BUTTON_RADIUS, c);
    };

    draw_btn(0, close_color);
    draw_btn(1, min_color);
    draw_btn(2, max_color);

    // Title text — muted gray, smaller scale, vertically centered
    if (!m_title.empty()) {
        const double text_w = static_cast<double>(m_title.length()) * 5.5; // approx at scale 1.0
        Point title_pos{f.x() + (f.width() - text_w) / 2.0, f.y() + (f.height() - 14.0) * 0.5};
        painter.draw_text(title_pos, m_title, TITLE_TEXT, 1.0);
    }
}

bool TitleBarWidget::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        Point p{event.pointer.x, event.pointer.y};
        int old_hover = m_hovered_button;
        m_hovered_button = -1;

        for (int i = 0; i < 3; ++i) {
            Rect r = button_rect(i);
            double lx = p.x - frame().left();
            double ly = p.y - frame().top();
            if (lx >= r.left() && lx <= r.right() && ly >= r.top() && ly <= r.bottom()) {
                m_hovered_button = i;
                break;
            }
        }

        if (old_hover != m_hovered_button) {
            mark_needs_paint();
            return true;
        }
    } else if (event.type == EventType::PointerButtonPress
               && event.pointer.button == MouseButton::Left) {
        // Only process click if it's within the TitleBar's bounds
        double px = event.pointer.x;
        double py = event.pointer.y;
        if (px < frame().left() || px > frame().right() || py < frame().top() || py > frame().bottom()) {
            return Widget::handle_event(event);
        }

        int hit_index = m_hovered_button;
        if (hit_index >= 0) {
            // Ignore drags on buttons
        } else if (m_on_move) {
            // Clicked on the title bar background -> trigger move grab
            m_on_move(event.pointer.serial);
            return true;
        }

        if (m_hovered_button == 0) {
            // ── Close ────────────────────────────────────────────────────────
            if (m_on_close) m_on_close();
            return true;
        } else if (m_hovered_button == 1) {
            // ── Minimize ─────────────────────────────────────────────────────
            // Sends xdg_toplevel.minimize to compositor; compositor hides the
            // scene node. No restore until a dock/taskbar is implemented.
            if (m_on_minimize) m_on_minimize();
            return true;
        } else if (m_hovered_button == 2) {
            // ── Maximize / Restore ───────────────────────────────────────────
            m_is_maximized = !m_is_maximized;
            if (m_on_maximize) m_on_maximize();
            mark_needs_paint(); // repaint to update any visual state
            return true;
        }
    }

    return Widget::handle_event(event);
}

} // namespace txui
