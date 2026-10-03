// ============================================================================
// wallpaper_dbus.cpp — D-Bus Service for io.tinexus.shell.Wallpaper
// ============================================================================
// Implements the D-Bus interface for tinexus-wallpaper via sd-bus.
// Pure C++20, zero Qt dependency.
// ============================================================================
#include "wallpaper/wallpaper_dbus.hpp"
#include "common/logger.hpp"
#include "common/DBusNames.hpp"

#include <cstring>
#include <cerrno>

namespace tinexus::wallpaper {

static const sd_bus_vtable wallpaper_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetWallpaper", "sybq", "", WallpaperDBus::method_set_wallpaper, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetStatus", "", "sybqqu", WallpaperDBus::method_get_status, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AdvanceFrame", "", "", WallpaperDBus::method_advance_frame, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("WallpaperChanged", "syb", 0),
    SD_BUS_PROPERTY("CurrentPath", "s", WallpaperDBus::get_property, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("FitMode", "y", WallpaperDBus::get_property, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("IsDynamic", "b", WallpaperDBus::get_property, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END
};

WallpaperDBus& WallpaperDBus::instance() {
    static WallpaperDBus s_instance;
    return s_instance;
}

WallpaperDBus::~WallpaperDBus() {
    shutdown();
}

bool WallpaperDBus::init(SetWallpaperHandler on_set,
                         StatusQueryHandler on_status,
                         AdvanceFrameHandler on_advance,
                         ReloadHandler on_reload) {
    m_on_set     = std::move(on_set);
    m_on_status  = std::move(on_status);
    m_on_advance = std::move(on_advance);
    m_on_reload  = std::move(on_reload);

    int r = sd_bus_open_user(&m_bus);
    if (r < 0 || !m_bus) {
        log::error("[wallpaper/dbus] Failed to connect to user session bus: {}", std::strerror(-r));
        return false;
    }

    r = sd_bus_add_object_vtable(m_bus,
                                 &m_slot_io,
                                 tinexus::common::dbus::path::Wallpaper,
                                 tinexus::common::dbus::interface::Wallpaper,
                                 wallpaper_vtable,
                                 this);
    if (r < 0) {
        log::error("[wallpaper/dbus] Failed to add {} vtable: {}",
                   tinexus::common::dbus::path::Wallpaper, std::strerror(-r));
        shutdown();
        return false;
    }

    // Also register on legacy paths for compatibility
    sd_bus_add_object_vtable(m_bus,
                             &m_slot_root,
                             tinexus::common::dbus::legacy::WallpaperPath,
                             tinexus::common::dbus::legacy::Wallpaper,
                             wallpaper_vtable,
                             this);
    sd_bus_add_object_vtable(m_bus,
                             nullptr,
                             "/Wallpaper",
                             tinexus::common::dbus::legacy::Wallpaper,
                             wallpaper_vtable,
                             this);

    r = sd_bus_request_name(m_bus, tinexus::common::dbus::service::Wallpaper, 0);
    if (r < 0) {
        log::warn("[wallpaper/dbus] Failed to acquire '{}': {}",
                  tinexus::common::dbus::service::Wallpaper, std::strerror(-r));
    } else {
        log::info("[wallpaper/dbus] Acquired service name '{}'",
                  tinexus::common::dbus::service::Wallpaper);
    }
    // Also acquire legacy service name
    sd_bus_request_name(m_bus, tinexus::common::dbus::legacy::Wallpaper, 0);

    // Subscribe to Settings signals for automatic theme/config reload
    r = sd_bus_match_signal(m_bus,
                            &m_match_theme,
                            nullptr,
                            nullptr,
                            tinexus::common::dbus::interface::Settings,
                            "ThemeChanged",
                            on_settings_signal,
                            this);
    if (r < 0) {
        log::warn("[wallpaper/dbus] Match ThemeChanged failed: {}", std::strerror(-r));
    }

    r = sd_bus_match_signal(m_bus,
                            &m_match_config,
                            nullptr,
                            nullptr,
                            tinexus::common::dbus::interface::Settings,
                            "ConfigChanged",
                            on_settings_signal,
                            this);
    if (r < 0) {
        log::warn("[wallpaper/dbus] Match ConfigChanged failed: {}", std::strerror(-r));
    }

    // Also subscribe to legacy Settings signals
    sd_bus_match_signal(m_bus,
                        nullptr,
                        nullptr,
                        nullptr,
                        tinexus::common::dbus::legacy::Settings,
                        "ThemeChanged",
                        on_settings_signal,
                        this);
    sd_bus_match_signal(m_bus,
                        nullptr,
                        nullptr,
                        nullptr,
                        tinexus::common::dbus::legacy::Settings,
                        "ConfigChanged",
                        on_settings_signal,
                        this);

    log::info("[wallpaper/dbus] {} registered on user session bus",
              tinexus::common::dbus::service::Wallpaper);
    return true;
}

void WallpaperDBus::shutdown() {
    if (m_match_theme)  { sd_bus_slot_unref(m_match_theme);  m_match_theme = nullptr; }
    if (m_match_config) { sd_bus_slot_unref(m_match_config); m_match_config = nullptr; }
    if (m_slot_root)    { sd_bus_slot_unref(m_slot_root);    m_slot_root = nullptr; }
    if (m_slot_io)      { sd_bus_slot_unref(m_slot_io);      m_slot_io = nullptr; }
    if (m_bus)          { sd_bus_flush_close_unref(m_bus);   m_bus = nullptr; }
}

int WallpaperDBus::fd() const noexcept {
    return m_bus ? sd_bus_get_fd(m_bus) : -1;
}

int WallpaperDBus::events() const noexcept {
    return m_bus ? sd_bus_get_events(m_bus) : 0;
}

void WallpaperDBus::process() {
    if (!m_bus) return;
    int r = 0;
    while ((r = sd_bus_process(m_bus, nullptr)) > 0) {
        // Drain pending D-Bus messages
    }
}

void WallpaperDBus::emit_wallpaper_changed(const std::string& path, uint8_t mode, bool dynamic) {
    update_state(path, mode, dynamic);
    if (!m_bus) return;

    sd_bus_emit_signal(m_bus,
                       tinexus::common::dbus::path::Wallpaper,
                       tinexus::common::dbus::interface::Wallpaper,
                       "WallpaperChanged",
                       "syb",
                       path.c_str(),
                       mode,
                       dynamic ? 1 : 0);

    sd_bus_emit_signal(m_bus,
                       tinexus::common::dbus::legacy::WallpaperPath,
                       tinexus::common::dbus::legacy::Wallpaper,
                       "WallpaperChanged",
                       "syb",
                       path.c_str(),
                       mode,
                       dynamic ? 1 : 0);

    sd_bus_emit_signal(m_bus,
                       "/Wallpaper",
                       tinexus::common::dbus::legacy::Wallpaper,
                       "WallpaperChanged",
                       "syb",
                       path.c_str(),
                       mode,
                       dynamic ? 1 : 0);

    log::info("[wallpaper/dbus] Emitted WallpaperChanged signal: path='{}', mode={}, dynamic={}",
              path, mode, dynamic ? 1 : 0);
}

int WallpaperDBus::method_set_wallpaper(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* self = static_cast<WallpaperDBus*>(userdata);
    const char* path = nullptr;
    uint8_t mode = 0;
    int dynamic = 0;
    uint16_t fade_ms = 0;

    int r = sd_bus_message_read(m, "sybq", &path, &mode, &dynamic, &fade_ms);
    if (r < 0) return r;

    log::info("[wallpaper/dbus] SetWallpaper received: path='{}', mode={}, dynamic={}, fadeMs={}",
              path ? path : "", mode, dynamic, fade_ms);

    if (self && self->m_on_set) {
        self->m_on_set(path ? path : "", mode, dynamic != 0, fade_ms);
    }

    return sd_bus_reply_method_return(m, "");
}

int WallpaperDBus::method_get_status(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* self = static_cast<WallpaperDBus*>(userdata);
    WallpaperStatus st{};
    if (self && self->m_on_status) {
        st = self->m_on_status();
    }
    return sd_bus_reply_method_return(m, "sybqqu",
                                      st.path.c_str(),
                                      st.mode,
                                      st.is_dynamic ? 1 : 0,
                                      st.current_frame_index,
                                      st.total_frames,
                                      st.next_change_secs);
}

int WallpaperDBus::method_advance_frame(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* self = static_cast<WallpaperDBus*>(userdata);
    log::info("[wallpaper/dbus] AdvanceFrame invoked");
    if (self && self->m_on_advance) {
        self->m_on_advance();
    }
    return sd_bus_reply_method_return(m, "");
}

int WallpaperDBus::get_property(sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
                                const char* property, sd_bus_message* reply, void* userdata,
                                sd_bus_error* /*ret_error*/) {
    auto* self = static_cast<WallpaperDBus*>(userdata);
    if (!self) return -EINVAL;

    if (std::strcmp(property, "CurrentPath") == 0) {
        return sd_bus_message_append(reply, "s", self->current_path().c_str());
    } else if (std::strcmp(property, "FitMode") == 0) {
        return sd_bus_message_append(reply, "y", self->fit_mode());
    } else if (std::strcmp(property, "IsDynamic") == 0) {
        return sd_bus_message_append(reply, "b", self->is_dynamic() ? 1 : 0);
    }
    return -EINVAL;
}

int WallpaperDBus::on_settings_signal(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* self = static_cast<WallpaperDBus*>(userdata);
    const char* member = sd_bus_message_get_member(m);
    log::info("[wallpaper/dbus] Received settings signal: {}", member ? member : "unknown");

    if (self && self->m_on_reload) {
        self->m_on_reload();
    }
    return 0;
}

} // namespace tinexus::wallpaper
