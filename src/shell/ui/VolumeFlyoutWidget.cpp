#include "shell/ui/VolumeFlyoutWidget.hpp"
#include "common/AudioUtils.hpp"
#include <txui/render/FontMetrics.hpp>

namespace tinexus::shell {

namespace {
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_SEC       {155, 162, 178, 220};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};
    constexpr txui::Color ACCENT_BLUE   { 59, 130, 246, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};
    constexpr txui::Color MUTE_RED      {239,  68,  68, 255};
}

VolumeFlyoutWidget::VolumeFlyoutWidget() {
    refresh_state();

    m_slider = txui::make_ref<txui::Slider>(0.0, 100.0, static_cast<double>(m_current_volume));
    m_slider->set_active_color(m_is_muted ? MUTE_RED : ACCENT_CYAN);
    m_slider->set_on_value_changed([this](double val) {
        m_current_volume = static_cast<int>(val);
        m_is_muted = (m_current_volume == 0);
        m_slider->set_active_color(m_is_muted ? MUTE_RED : ACCENT_CYAN);
        hardware::AudioUtils::set_volume_percent(m_current_volume, /*persist=*/false, /*throttle=*/true);
        if (m_mute_btn) {
            m_mute_btn->set_text(m_is_muted ? "Unmute" : "Mute");
        }
        mark_needs_paint();
    });

    m_mute_btn = txui::make_ref<txui::Button>(m_is_muted ? "Unmute" : "Mute");
    m_mute_btn->set_on_click([this]() {
        m_is_muted = hardware::AudioUtils::toggle_mute(/*persist=*/true);
        m_mute_btn->set_text(m_is_muted ? "Unmute" : "Mute");
        m_slider->set_active_color(m_is_muted ? MUTE_RED : ACCENT_CYAN);
        mark_needs_paint();
    });

    m_test_btn = txui::make_ref<txui::Button>("Test Audio");
    m_test_btn->set_on_click([]() {
        hardware::AudioUtils::play_chime();
    });

    add_child(m_slider);
    add_child(m_mute_btn);
    add_child(m_test_btn);
}

void VolumeFlyoutWidget::refresh_state() {
    m_control_name = hardware::AudioUtils::detect_primary_control();
    m_current_volume = hardware::AudioUtils::get_volume_percent();
    m_is_muted = hardware::AudioUtils::is_muted();
    if (m_slider) {
        m_slider->set_value(static_cast<double>(m_current_volume));
        m_slider->set_active_color(m_is_muted ? MUTE_RED : ACCENT_CYAN);
    }
    if (m_mute_btn) {
        m_mute_btn->set_text(m_is_muted ? "Unmute" : "Mute");
    }
}

txui::Size VolumeFlyoutWidget::measure_override(const txui::Constraints&) noexcept {
    return txui::Size(240.0, 140.0);
}

void VolumeFlyoutWidget::layout_override(const txui::Rect& f) noexcept {
    if (m_slider) {
        m_slider->layout(txui::Rect(f.x() + 16.0, f.y() + 48.0, f.width() - 32.0, 24.0));
    }
    if (m_mute_btn) {
        m_mute_btn->layout(txui::Rect(f.x() + 16.0, f.y() + 86.0, 96.0, 32.0));
    }
    if (m_test_btn) {
        m_test_btn->layout(txui::Rect(f.x() + 120.0, f.y() + 86.0, f.width() - 136.0, 32.0));
    }
}

void VolumeFlyoutWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double mx = f.x(), my = f.y();
    const double mw = f.width(), mh = f.height();

    // 1. Drop shadow
    painter.fill_rounded_rect(txui::Rect(mx - 4.0, my + 6.0, mw + 8.0, mh + 4.0), 14.0, txui::Color(0, 0, 0, 95));

    // 2. Glassmorphic card body
    painter.fill_rounded_rect(txui::Rect(mx, my, mw, mh), 14.0, CARD_BG);
    painter.fill_rounded_rect(txui::Rect(mx - 1.0, my - 1.0, mw + 2.0, mh + 2.0), 15.0, BORDER_LINE);

    // 3. Header title & control device
    painter.draw_text(txui::Point(mx + 16.0, my + 14.0), "Sound Output", TXT_PRI, 13.5, true);
    std::string dev_label = "(" + m_control_name + ")";
    painter.draw_text(txui::Point(mx + 112.0, my + 16.0), dev_label, TXT_DIM, 11.0);

    // 4. Percentage indicator
    std::string pct_str = m_is_muted ? "Muted" : (std::to_string(m_current_volume) + "%");
    txui::Color pct_col = m_is_muted ? MUTE_RED : ACCENT_CYAN;
    painter.draw_text(txui::Point(mx + mw - 56.0, my + 14.0), pct_str, pct_col, 12.5, true);

    // 5. Paint child controls
    if (m_slider)   m_slider->paint(painter);
    if (m_mute_btn) m_mute_btn->paint(painter);
    if (m_test_btn) m_test_btn->paint(painter);
}

bool VolumeFlyoutWidget::handle_event(const txui::Event& event) noexcept {
    bool handled = false;
    if (m_slider && m_slider->handle_event(event)) handled = true;
    if (m_mute_btn && m_mute_btn->handle_event(event)) handled = true;
    if (m_test_btn && m_test_btn->handle_event(event)) handled = true;

    // On mouse release, ensure final value is persisted to hardware.toml
    if (event.type == txui::EventType::PointerButtonRelease) {
        hardware::AudioUtils::set_volume_percent(m_current_volume, /*persist=*/true, /*throttle=*/false);
    }

    return handled;
}

} // namespace tinexus::shell
