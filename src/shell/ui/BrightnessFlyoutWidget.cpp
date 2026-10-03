#include "shell/ui/BrightnessFlyoutWidget.hpp"
#include "common/BacklightUtils.hpp"
#include <txui/render/FontMetrics.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace tinexus::shell {

namespace {
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_SEC       {155, 162, 178, 220};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};
    constexpr txui::Color ACCENT_AMBER  {245, 158,  11, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};
}

BrightnessFlyoutWidget::BrightnessFlyoutWidget() {
    refresh_state();

    m_slider = txui::make_ref<txui::Slider>(5.0, 100.0, static_cast<double>(m_current_brightness));
    m_slider->set_active_color(ACCENT_AMBER);
    m_slider->set_on_value_changed([this](double val) {
        m_current_brightness = static_cast<int>(val);
        hardware::BacklightUtils::set_brightness_percent(m_current_brightness, /*persist=*/false, /*throttle=*/true);
        mark_needs_paint();
    });

    m_theme_btn = txui::make_ref<txui::Button>(m_dark_theme ? "Dark Mode: On" : "Light Mode");
    m_theme_btn->set_on_click([this]() {
        m_dark_theme = !m_dark_theme;
        m_theme_btn->set_text(m_dark_theme ? "Dark Mode: On" : "Light Mode");
        mark_needs_paint();
    });

    add_child(m_slider);
    add_child(m_theme_btn);
}

void BrightnessFlyoutWidget::refresh_state() {
    std::string raw_dev = hardware::BacklightUtils::detect_backlight_device();
    m_device_name = raw_dev.empty() ? "Display" : fs::path(raw_dev).filename().string();
    m_current_brightness = hardware::BacklightUtils::get_brightness_percent();
    if (m_slider) {
        m_slider->set_value(static_cast<double>(m_current_brightness));
    }
}

txui::Size BrightnessFlyoutWidget::measure_override(const txui::Constraints&) noexcept {
    return txui::Size(240.0, 135.0);
}

void BrightnessFlyoutWidget::layout_override(const txui::Rect& f) noexcept {
    if (m_slider) {
        m_slider->layout(txui::Rect(f.x() + 16.0, f.y() + 48.0, f.width() - 32.0, 24.0));
    }
    if (m_theme_btn) {
        m_theme_btn->layout(txui::Rect(f.x() + 16.0, f.y() + 86.0, f.width() - 32.0, 32.0));
    }
}

void BrightnessFlyoutWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double mx = f.x(), my = f.y();
    const double mw = f.width(), mh = f.height();

    // 1. Drop shadow
    painter.fill_rounded_rect(txui::Rect(mx - 4.0, my + 6.0, mw + 8.0, mh + 4.0), 14.0, txui::Color(0, 0, 0, 95));

    // 2. Glassmorphic card body
    painter.fill_rounded_rect(txui::Rect(mx, my, mw, mh), 14.0, CARD_BG);
    painter.fill_rounded_rect(txui::Rect(mx - 1.0, my - 1.0, mw + 2.0, mh + 2.0), 15.0, BORDER_LINE);

    // 3. Header title & detected device
    painter.draw_text(txui::Point(mx + 16.0, my + 14.0), "Brightness", TXT_PRI, 13.5, true);
    std::string dev_label = "(" + m_device_name + ")";
    painter.draw_text(txui::Point(mx + 96.0, my + 16.0), dev_label, TXT_DIM, 11.0);

    // 4. Percentage indicator
    std::string pct_str = std::to_string(m_current_brightness) + "%";
    painter.draw_text(txui::Point(mx + mw - 50.0, my + 14.0), pct_str, ACCENT_AMBER, 12.5, true);

    // 5. Paint child controls
    if (m_slider)    m_slider->paint(painter);
    if (m_theme_btn) m_theme_btn->paint(painter);
}

bool BrightnessFlyoutWidget::handle_event(const txui::Event& event) noexcept {
    bool handled = false;
    if (m_slider && m_slider->handle_event(event)) handled = true;
    if (m_theme_btn && m_theme_btn->handle_event(event)) handled = true;

    // On mouse release, ensure final value is persisted to hardware.toml
    if (event.type == txui::EventType::PointerButtonRelease) {
        hardware::BacklightUtils::set_brightness_percent(m_current_brightness, /*persist=*/true, /*throttle=*/false);
    }

    return handled;
}

} // namespace tinexus::shell
