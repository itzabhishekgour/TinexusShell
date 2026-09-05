#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/core/Types.hpp>
#include <txui/math/Rect.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::settings_ui {

enum class SettingsPage {
    Display,
    Personalization,
    Network,
    System,
    KeyboardShortcuts,
    PrivacySecurity,
    About
};

struct AccentOption  { uint8_t r, g, b; std::string name; };
struct WallpaperItem { std::string name, path; uint8_t preview_r{20}, preview_g{30}, preview_b{50}; };
struct NetworkIface  { std::string name, state, ip4_addr; uint64_t rx_mb{0}, tx_mb{0}; };

class SettingsWidget : public txui::Widget {
public:
    SettingsWidget();
    void select_page(SettingsPage page);
    bool handle_event(const txui::Event& event) noexcept override;

protected:
    txui::Size measure_override(const txui::Constraints& c) noexcept override;
    void       layout_override(const txui::Rect& frame)    noexcept override;
    void       paint_override(txui::Painter& painter)      const noexcept override;

private:
    // ── Navigation ────────────────────────────────────────────────────────
    SettingsPage m_current_page{SettingsPage::Display};
    int          m_hovered_tab{-1};

    txui::Rect m_sidebar_rect;
    txui::Rect m_content_rect;
    static constexpr txui::float64 SIDEBAR_W    = 222.0;
    static constexpr txui::float64 ITEM_H       = 44.0;
    static constexpr int           SIDEBAR_PAGES = 7;

    // ── System Info ───────────────────────────────────────────────────────
    std::string m_os_version, m_mem_info, m_cpu_model, m_comp_info;

    // ── Display ───────────────────────────────────────────────────────────
    bool m_night_light_enabled{false};
    bool m_vrr_enabled{false};
    int  m_display_scale_idx{0};

    // ── Personalization ───────────────────────────────────────────────────
    int         m_selected_accent_idx{0};
    std::string m_theme_mode{"Dark"};
    int         m_selected_wallpaper_idx{0};
    std::vector<WallpaperItem> m_wallpapers;

    // ── System/Power ──────────────────────────────────────────────────────
    int         m_screen_timeout_min{5};
    int         m_sleep_after_min{15};
    int         m_power_profile_idx{1};   // 0=Power Saver, 1=Balanced, 2=Performance
    bool        m_lock_on_sleep{true};
    bool        m_pam_auth{true};
    std::string m_session_status_msg;

    // ── Clipboard (stored in TOML; tinexus-clip daemon history is Phase 2) ─
    bool m_clipboard_enabled{true};
    int  m_clipboard_history_size{50};

    // ── Network (live /sys reads, no popen) ───────────────────────────────
    std::vector<NetworkIface> m_network_ifaces;
    void scan_network_ifaces();

    // ── Privacy/Security ──────────────────────────────────────────────────
    struct UnverifiedApp {
        std::string name, path, hash;
        bool is_hovered{false};
        txui::Rect btn_rect;
    };
    std::vector<UnverifiedApp> m_unverified_apps;
    mutable txui::Rect m_rescan_btn_rect;
    bool m_rescan_hovered{false};
    void refresh_unverified_apps();
    void trust_app(const std::string& hash);

    // ── Sidebar paint ─────────────────────────────────────────────────────
    void paint_sidebar(txui::Painter& p) const noexcept;
    void paint_sidebar_item(txui::Painter& p, const char* label,
                             SettingsPage page, txui::float64 y) const noexcept;

    // ── Icon glyphs (drawn relative to 28×28 tile origin tx, ty) ─────────
    void draw_icon_display        (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_personalization(txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_network        (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_system         (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_keyboard       (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_privacy        (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;
    void draw_icon_about          (txui::Painter& p, txui::float64 tx, txui::float64 ty) const noexcept;

    // ── Content pages ─────────────────────────────────────────────────────
    void paint_display_page            (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_personalization_page    (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_network_page            (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_system_page             (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_keyboard_shortcuts_page (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_privacy_security_page   (txui::Painter& p, const txui::Rect& area) const noexcept;
    void paint_about_page              (txui::Painter& p, const txui::Rect& area) const noexcept;

    // ── Backend helpers ───────────────────────────────────────────────────
    void load_config();
    void save_config();
    void scan_wallpapers();
    void read_compositor_info();
    void trigger_session_action(int idx);
};

} // namespace tinexus::settings_ui

