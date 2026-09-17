// ============================================================================
// wallpaper_dbus.hpp — D-Bus Service for io.tinexus.Wallpaper
// ============================================================================
// Implements the D-Bus interface for tinexus-wallpaper via sd-bus.
// Pure C++20, zero Qt dependency.
// ============================================================================
#pragma once

#include <systemd/sd-bus.h>
#include <string>
#include <functional>
#include <cstdint>

namespace tinexus::wallpaper {

struct WallpaperStatus {
    std::string path;
    uint8_t     mode{0};
    bool        is_dynamic{false};
    uint16_t    current_frame_index{0};
    uint16_t    total_frames{1};
    uint32_t    next_change_secs{0};
};

class WallpaperDBus {
public:
    using SetWallpaperHandler = std::function<void(const std::string& path, uint8_t mode, bool dynamic, uint16_t fade_ms)>;
    using StatusQueryHandler  = std::function<WallpaperStatus()>;
    using AdvanceFrameHandler = std::function<void()>;
    using ReloadHandler       = std::function<void()>;

    static WallpaperDBus& instance();

    bool init(SetWallpaperHandler on_set,
              StatusQueryHandler on_status,
              AdvanceFrameHandler on_advance,
              ReloadHandler on_reload);
    void shutdown();

    [[nodiscard]] sd_bus* bus() const noexcept { return m_bus; }
    [[nodiscard]] int fd() const noexcept;
    [[nodiscard]] int events() const noexcept;
    void process();

    void emit_wallpaper_changed(const std::string& path, uint8_t mode, bool dynamic);

    void update_state(const std::string& path, uint8_t mode, bool dynamic) noexcept {
        m_current_path = path;
        m_fit_mode     = mode;
        m_is_dynamic   = dynamic;
    }

    [[nodiscard]] const std::string& current_path() const noexcept { return m_current_path; }
    [[nodiscard]] uint8_t fit_mode() const noexcept { return m_fit_mode; }
    [[nodiscard]] bool is_dynamic() const noexcept { return m_is_dynamic; }

    // Static sd-bus callbacks
    static int method_set_wallpaper(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_get_status(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int method_advance_frame(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
    static int get_property(sd_bus* bus, const char* path, const char* interface,
                            const char* property, sd_bus_message* reply, void* userdata,
                            sd_bus_error* ret_error);
    static int on_settings_signal(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);

private:
    WallpaperDBus() = default;
    ~WallpaperDBus();

    sd_bus*      m_bus{nullptr};
    sd_bus_slot* m_slot_io{nullptr};
    sd_bus_slot* m_slot_root{nullptr};
    sd_bus_slot* m_match_theme{nullptr};
    sd_bus_slot* m_match_config{nullptr};

    SetWallpaperHandler m_on_set;
    StatusQueryHandler  m_on_status;
    AdvanceFrameHandler m_on_advance;
    ReloadHandler       m_on_reload;

    std::string m_current_path;
    uint8_t     m_fit_mode{0};
    bool        m_is_dynamic{false};
};

} // namespace tinexus::wallpaper
