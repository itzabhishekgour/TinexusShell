#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Types.hpp>
#include <string>

namespace tinexus::settings_ui {

enum class SettingsPage {
    Display,
    Personalization,
    System,
    About
};

// ─────────────────────────────────────────────────────────────────────────────
// SettingsWidget — The Tinexus Control Center root widget.
//
// Visual Design:
//   ┌────────────────────────────────────────────────────────────────────────┐
//   │ Sidebar (220px)  │ Content Area (fill)                                │
//   │ ─────────────────┤────────────────────────────────────────────────────│
//   │ 🖥  Display       │ [Page content with cards, toggles, sliders]        │
//   │ 🎨 Personalize   │                                                    │
//   │ ⚙️  System        │                                                    │
//   │ ℹ  About         │                                                    │
//   └────────────────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class SettingsWidget : public txui::Widget {
public:
    SettingsWidget();

    void select_page(SettingsPage page);

    bool handle_event(const txui::Event& event) noexcept override;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    SettingsPage m_current_page{SettingsPage::Display};
    int m_hovered_tab{-1};

    // Sidebar
    void paint_sidebar(txui::Painter& painter) const noexcept;
    void paint_sidebar_item(txui::Painter& painter, const std::string& label,
                            SettingsPage page, txui::float64 y) const noexcept;

    // Content pages
    void paint_display_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_personalization_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_system_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_about_page(txui::Painter& painter, const txui::Rect& area) const noexcept;

    // Rects updated by layout
    txui::Rect m_sidebar_rect;
    txui::Rect m_content_rect;

    static constexpr txui::float64 SIDEBAR_W = 220.0;
    static constexpr txui::float64 ITEM_H    = 48.0;
    static constexpr txui::float64 ITEM_Y0   = 80.0; // below title

    // System State
    std::string m_os_version;
    std::string m_mem_info;
    std::string m_cpu_model;
    
    // Config State
    int m_screen_timeout_min{5};
    int m_sleep_after_min{15};
    std::string m_power_profile{"Balanced"};
};

} // namespace tinexus::settings_ui
