#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Types.hpp>
#include <txui/math/Rect.hpp>
#include <string>
#include <vector>

namespace tinexus::settings_ui {

enum class SettingsPage {
    Display,
    Personalization,
    System,
    PrivacySecurity,
    About
};

struct AccentOption {
    uint8_t r, g, b;
    std::string name;
};

struct WallpaperItem {
    std::string name;
    std::string path;
    uint8_t preview_r{20};
    uint8_t preview_g{30};
    uint8_t preview_b{50};
};

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

    // Sidebar rendering
    void paint_sidebar(txui::Painter& painter) const noexcept;
    void paint_sidebar_item(txui::Painter& painter, const std::string& label,
                            SettingsPage page, txui::float64 y) const noexcept;

    void draw_icon_display(txui::Painter& painter, txui::float64 cx, txui::float64 cy) const noexcept;
    void draw_icon_personalization(txui::Painter& painter, txui::float64 cx, txui::float64 cy) const noexcept;
    void draw_icon_system(txui::Painter& painter, txui::float64 cx, txui::float64 cy) const noexcept;
    void draw_icon_privacy(txui::Painter& painter, txui::float64 cx, txui::float64 cy) const noexcept;
    void draw_icon_about(txui::Painter& painter, txui::float64 cx, txui::float64 cy) const noexcept;

    // Content pages
    void paint_display_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_personalization_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_system_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_privacy_security_page(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_about_page(txui::Painter& painter, const txui::Rect& area) const noexcept;

    // Layout Rects
    txui::Rect m_sidebar_rect;
    txui::Rect m_content_rect;

    static constexpr txui::float64 SIDEBAR_W = 220.0;
    static constexpr txui::float64 ITEM_H    = 48.0;
    static constexpr txui::float64 ITEM_Y0   = 124.0;

    // System Information
    std::string m_os_version;
    std::string m_mem_info;
    std::string m_cpu_model;
    
    // Display Page Settings
    bool m_night_light_enabled{false};
    bool m_vrr_enabled{false};
    int m_display_scale_idx{0}; // 0: 100%, 1: 125%, 2: 150%, 3: 200%

    // Personalization Page Settings
    int m_selected_accent_idx{0};
    std::string m_theme_mode{"Dark"}; // "Dark" or "Light"
    int m_selected_wallpaper_idx{0};
    std::vector<WallpaperItem> m_wallpapers;

    // System Page Settings
    int m_screen_timeout_min{5};
    int m_sleep_after_min{15};
    std::string m_power_profile{"Balanced"}; // "Power Saver", "Balanced", "Performance"
    bool m_lock_on_sleep{true};
    bool m_pam_auth{true};

    // Session Trigger Feedback
    std::string m_session_status_msg;

    // Privacy & Security State
    struct UnverifiedApp {
        std::string name;
        std::string path;
        std::string hash;
        bool is_hovered{false};
        txui::Rect btn_rect;
    };
    std::vector<UnverifiedApp> m_unverified_apps;
    void refresh_unverified_apps();
    void trust_app(const std::string& hash);

    // Helpers
    void load_config();
    void save_config();
    void scan_wallpapers();
    void trigger_session_action(int action_idx);
};

} // namespace tinexus::settings_ui
