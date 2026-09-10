#include "comp/backend/backend.hpp"
#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"
#include "comp/output/output.hpp"
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#include <wlr/render/pixman.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_data_device.h>
#define namespace wl_namespace
#include <wlr/types/wlr_layer_shell_v1.h>
#define static
#include <wlr/types/wlr_scene.h>
#undef static
#include <wlr/types/wlr_xdg_shell.h>
#undef namespace
#include <wlr/xcursor.h>
#include <wlr/util/edges.h>
#include <xkbcommon/xkbcommon.h>
}

#include "comp/input/seat_manager.hpp"
#include "comp/input/shortcut_engine.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/focus/focus_manager.hpp"
#include "comp/server/server.hpp"
#include "comp/window/window_state.hpp"
#include "comp/animation/animation_manager.hpp"
#include "common/AppId.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <sys/wait.h>

namespace tinexus::comp {

using tinexus::common::get_canonical_app_id;
using tinexus::common::is_single_instance_app;

namespace {
void wlroots_log_callback(enum wlr_log_importance importance, const char *fmt, va_list args) {
    char buf[1024];
    va_list args_copy;
    va_copy(args_copy, args);
    vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    fprintf(stderr, "[wlr] %s\n", buf);
    fflush(stderr);

    switch (importance) {
        case WLR_ERROR:
            tinexus::log::error("[wlr] {}", buf);
            break;
        case WLR_INFO:
            tinexus::log::info("[wlr] {}", buf);
            break;
        case WLR_DEBUG:
        default:
            tinexus::log::debug("[wlr] {}", buf);
            break;
    }
}
} // namespace

class WlrootsBackend : public Backend {
public:
    explicit WlrootsBackend(struct wl_display* display) : m_display(display) {}

    ~WlrootsBackend() override {
        shutdown();
    }

    bool initialize() override {
        // Initialize wlroots logging early with maximum verbosity
        wlr_log_init(WLR_DEBUG, wlroots_log_callback);
        log::info("[Backend] wlroots logging initialized at WLR_DEBUG level.");

        // Diagnostic inspection of DRI devices
        if (std::filesystem::exists("/dev/dri")) {
            log::info("[Backend] Scanning /dev/dri directory:");
            for (const auto& entry : std::filesystem::directory_iterator("/dev/dri")) {
                log::info("[Backend]   Device node: {}", entry.path().string());
            }
        } else {
            log::warn("[Backend] /dev/dri directory does not exist!");
        }

        if (std::filesystem::exists("/sys/class/drm")) {
            log::info("[Backend] Scanning /sys/class/drm connectors/cards:");
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm")) {
                log::info("[Backend]   DRM sysfs: {}", entry.path().filename().string());
            }
        } else {
            log::warn("[Backend] /sys/class/drm does not exist!");
        }

        log::info("[Backend] Creating wlroots backend...");

        // 1. Create the wlroots backend
        // In wlroots 0.18, the signature takes a wl_event_loop. Wait, actually wlr_backend_autocreate usually takes a wl_event_loop and a session.
        // Let's use wl_display_get_event_loop(m_display).
        m_wlr_backend = wlr_backend_autocreate(wl_display_get_event_loop(m_display), nullptr);
        if (!m_wlr_backend) {
            log::error("[Backend] Failed to autocreate wlroots backend.");
            return false;
        }

        // 2. Create renderer
        m_wlr_renderer = wlr_renderer_autocreate(m_wlr_backend);
        if (!m_wlr_renderer) {
            log::error("[Renderer] Failed to autocreate wlroots renderer.");
            return false;
        }

        std::string active_renderer_str = "pixman";
        if (wlr_renderer_is_gles2(m_wlr_renderer)) {
            active_renderer_str = "gles2";
            log::info("[Renderer] wlroots active renderer: OpenGL ES 2 (Hardware Accelerated)");
        } else if (wlr_renderer_is_pixman(m_wlr_renderer)) {
            active_renderer_str = "pixman";
            log::warn("[Renderer] wlroots active renderer: Pixman (Software Rasterizer) — Hardware acceleration not available or failed");
        } else {
            active_renderer_str = "custom";
            log::info("[Renderer] wlroots active renderer: Custom / Autocreated");
        }

        // Publish live active renderer state to runtime dir and /run/tinexus/renderer so UI profilers
        // (About Tinexus, Settings, Monitor) can authoritatively read live compositor state
        try {
            tinexus::common::RuntimePaths::ensure_runtime_dir();
            std::string run_path = tinexus::common::RuntimePaths::get_runtime_dir() + "/renderer";
            std::ofstream rf(run_path);
            if (rf.is_open()) {
                rf << active_renderer_str << "\n";
            }
            std::filesystem::create_directories("/run/tinexus");
            std::ofstream rf_legacy("/run/tinexus/renderer");
            if (rf_legacy.is_open()) {
                rf_legacy << active_renderer_str << "\n";
            }
        } catch (...) {}

        wlr_renderer_init_wl_display(m_wlr_renderer, m_display);

        // 3. Create allocator
        m_wlr_allocator = wlr_allocator_autocreate(m_wlr_backend, m_wlr_renderer);
        if (!m_wlr_allocator) {
            log::error("[Backend] Failed to autocreate wlroots allocator.");
            return false;
        }

        // 4. Create compositor
        m_wlr_compositor = wlr_compositor_create(m_display, 6, m_wlr_renderer);
        if (!m_wlr_compositor) {
            log::error("[Backend] Failed to create wlroots compositor.");
            return false;
        }

        wlr_subcompositor_create(m_display);

        // 5. Create wlr_shm with renderer (critical for SHM buffer type recognition)
        // wl_display_init_shm alone doesn't register renderer-supported formats
        m_wlr_shm = wlr_shm_create_with_renderer(m_display, 1, m_wlr_renderer);
        if (!m_wlr_shm) {
            log::error("[Backend] Failed to create wlr_shm with renderer.");
            return false;
        }

        log::info("[Backend] Backend initialized successfully.");

        m_output_layout = wlr_output_layout_create(m_display);

        // Create wlr_scene and scene tree structure
        m_scene = wlr_scene_create();
        wlr_scene_attach_output_layout(m_scene, m_output_layout);

        m_scene_tree_background = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_bottom     = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_normal     = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_top        = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_overlay    = wlr_scene_tree_create(&m_scene->tree);

        // Bind FocusManager — scene + seat must both exist before this call.
        // (seat is created just below; bind_focus() call placed after seat init)
        // Actual bind happens after m_seat is assigned (see below).

        m_seat = wlr_seat_create(m_display, "seat0");
        SeatManager::instance().bind_seat(m_seat, "seat0");

        m_cursor = wlr_cursor_create();
        wlr_cursor_attach_output_layout(m_cursor, m_output_layout);
        m_cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);
        wlr_xcursor_manager_load(m_cursor_mgr, 1.0f);
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");

        // Bind FocusManager — both m_seat and m_scene are now ready.
        FocusManager::instance().bind(m_seat, m_scene);

        m_layer_shell = wlr_layer_shell_v1_create(m_display, 4);

        m_new_layer_surface_listener.notify = handle_new_layer_surface;
        wl_signal_add(&m_layer_shell->events.new_surface, &m_new_layer_surface_listener);

        m_xdg_shell = wlr_xdg_shell_create(m_display, 3);
        m_new_xdg_surface_listener.notify = handle_new_xdg_toplevel;
        wl_signal_add(&m_xdg_shell->events.new_toplevel, &m_new_xdg_surface_listener);

        m_new_output_listener.notify = handle_new_output;
        wl_signal_add(&m_wlr_backend->events.new_output, &m_new_output_listener);

        m_new_input_listener.notify = handle_new_input;
        wl_signal_add(&m_wlr_backend->events.new_input, &m_new_input_listener);

        m_cursor_motion_listener.notify = handle_cursor_motion;
        wl_signal_add(&m_cursor->events.motion, &m_cursor_motion_listener);

        m_cursor_motion_absolute_listener.notify = handle_cursor_motion_absolute;
        wl_signal_add(&m_cursor->events.motion_absolute, &m_cursor_motion_absolute_listener);

        m_cursor_button_listener.notify = handle_cursor_button;
        wl_signal_add(&m_cursor->events.button, &m_cursor_button_listener);

        m_cursor_axis_listener.notify = handle_cursor_axis;
        wl_signal_add(&m_cursor->events.axis, &m_cursor_axis_listener);

        m_cursor_frame_listener.notify = handle_cursor_frame;
        wl_signal_add(&m_cursor->events.frame, &m_cursor_frame_listener);

        // Create wl_data_device_manager so Wayland clients (foot, etc.) can bind
        // clipboard / drag-and-drop protocol. Without this, clients report
        // "no clipboard available" even though the global appears in registry.
        m_data_device_manager = wlr_data_device_manager_create(m_display);
        if (!m_data_device_manager) {
            log::warn("[Backend] wlr_data_device_manager_create failed — clipboard unavailable.");
        } else {
            log::info("[Backend] wl_data_device_manager created (clipboard enabled).");
        }

        return true;
    }

    bool start() override {
        log::info("[Backend] Starting wlroots backend...");
        AnimationManager::instance().set_frame_scheduler([this]() {
            for (auto& out : m_outputs) {
                if (out && out->get_wlr_output()) {
                    wlr_output_schedule_frame(out->get_wlr_output());
                }
            }
        });
        if (!wlr_backend_start(m_wlr_backend)) {
            log::error("[Backend] Failed to start wlroots backend.");
            return false;
        }
        log::info("[Backend] wlroots backend started successfully.");
        return true;
    }

    void stop() override {
        log::info("[Backend] Stopping wlroots backend...");
    }

    void shutdown() override {
        if (m_wlr_backend) {
            if (m_new_output_listener.link.next) { wl_list_remove(&m_new_output_listener.link); m_new_output_listener.link.next = nullptr; }
            if (m_new_input_listener.link.next) { wl_list_remove(&m_new_input_listener.link); m_new_input_listener.link.next = nullptr; }
            if (m_cursor_motion_listener.link.next) { wl_list_remove(&m_cursor_motion_listener.link); m_cursor_motion_listener.link.next = nullptr; }
            if (m_cursor_motion_absolute_listener.link.next) { wl_list_remove(&m_cursor_motion_absolute_listener.link); m_cursor_motion_absolute_listener.link.next = nullptr; }
            if (m_cursor_button_listener.link.next) { wl_list_remove(&m_cursor_button_listener.link); m_cursor_button_listener.link.next = nullptr; }
            if (m_cursor_axis_listener.link.next) { wl_list_remove(&m_cursor_axis_listener.link); m_cursor_axis_listener.link.next = nullptr; }
            if (m_cursor_frame_listener.link.next) { wl_list_remove(&m_cursor_frame_listener.link); m_cursor_frame_listener.link.next = nullptr; }
            if (m_new_layer_surface_listener.link.next) { wl_list_remove(&m_new_layer_surface_listener.link); m_new_layer_surface_listener.link.next = nullptr; }
            if (m_new_xdg_surface_listener.link.next) { wl_list_remove(&m_new_xdg_surface_listener.link); m_new_xdg_surface_listener.link.next = nullptr; }

            if (m_scene) {
                wlr_scene_node_destroy(&m_scene->tree.node);
                m_scene = nullptr;
            }

            if (m_cursor_mgr) wlr_xcursor_manager_destroy(m_cursor_mgr);
            if (m_cursor) wlr_cursor_destroy(m_cursor);
            if (m_output_layout) wlr_output_layout_destroy(m_output_layout);

            m_outputs.clear();
            wlr_backend_destroy(m_wlr_backend);
            m_wlr_backend = nullptr;
        }
        log::info("[Backend] Shutting down wlroots backend...");
    }

    struct wl_display* display() override {
        return m_display;
    }

    BackendType type() const noexcept override { return BackendType::Wlroots; }

    void set_locked(bool locked) noexcept {
        m_is_locked = locked;
        log::info("[Backend] Session lock state: {}", locked ? "LOCKED" : "UNLOCKED");
    }

    bool is_locked() const noexcept { return m_is_locked; }


private:
    struct ToplevelWrapper;
    struct wl_display* m_display{nullptr};
    struct wlr_backend* m_wlr_backend{nullptr};
    struct wlr_renderer* m_wlr_renderer{nullptr};
    struct wlr_allocator* m_wlr_allocator{nullptr};
    struct wlr_compositor* m_wlr_compositor{nullptr};
    struct wlr_shm* m_wlr_shm{nullptr};
    struct wlr_data_device_manager* m_data_device_manager{nullptr};

    struct wlr_output_layout* m_output_layout{nullptr};
    struct wlr_seat* m_seat{nullptr};
    struct wlr_cursor* m_cursor{nullptr};
    struct wlr_xcursor_manager* m_cursor_mgr{nullptr};
    struct wlr_layer_shell_v1* m_layer_shell{nullptr};
    struct wlr_xdg_shell* m_xdg_shell{nullptr};

    struct wlr_scene* m_scene{nullptr};
    struct wlr_scene_tree* m_scene_tree_background{nullptr};
    struct wlr_scene_tree* m_scene_tree_bottom{nullptr};
    struct wlr_scene_tree* m_scene_tree_normal{nullptr};
    struct wlr_scene_tree* m_scene_tree_top{nullptr};
    struct wlr_scene_tree* m_scene_tree_overlay{nullptr};

    struct wl_listener m_new_output_listener;
    struct wl_listener m_new_input_listener;
    struct wl_listener m_cursor_motion_listener;
    struct wl_listener m_cursor_motion_absolute_listener;
    struct wl_listener m_cursor_button_listener;
    struct wl_listener m_cursor_axis_listener;
    struct wl_listener m_cursor_frame_listener;
    struct wl_listener m_new_layer_surface_listener;
    struct wl_listener m_new_xdg_surface_listener;

    std::vector<std::unique_ptr<TinexusOutput>> m_outputs;
    ToplevelWrapper* m_active_toplevel{nullptr};
    struct wlr_surface* m_lock_surface{nullptr};

    struct KeyboardWrapper {
        struct wl_listener modifiers;
        struct wl_listener key;
        struct wl_listener destroy;
        struct wlr_keyboard* keyboard;
        WlrootsBackend* backend;
    };
    std::vector<std::unique_ptr<KeyboardWrapper>> m_keyboards;

    struct PointerWrapper {
        struct wl_listener destroy;
        struct wlr_input_device* device;
        WlrootsBackend* backend;
    };
    std::vector<std::unique_ptr<PointerWrapper>> m_pointers;

    // Session lock state — when true, ALL global shortcuts are suppressed
    // and keyboard input goes exclusively to the lock client
    bool m_is_locked{false};

    // Tracks the Wayland socket name so child processes inherit WAYLAND_DISPLAY
    std::string m_wayland_socket{};

    enum class CursorMode { Passthrough, Move, Resize };
    CursorMode m_cursor_mode{CursorMode::Passthrough};
    ToplevelWrapper* m_grabbed_toplevel{nullptr};
    double m_grab_x{0.0};
    double m_grab_y{0.0};
    int m_grab_geo_x{0};
    int m_grab_geo_y{0};
    struct wlr_box m_grab_geobox{};
    uint32_t m_grab_edges{0};
    SnapMode m_pending_snap{SnapMode::None};
    struct LayerSurfaceWrapper {
        struct wlr_layer_surface_v1* layer_surface{nullptr};
        struct wlr_scene_layer_surface_v1* scene_layer{nullptr};
        struct wl_listener destroy;
        struct wl_listener commit;
        WlrootsBackend* backend{nullptr};
    };

    struct ToplevelWrapper {
        struct wlr_xdg_toplevel* toplevel{nullptr};
        struct wlr_scene_tree* scene_tree{nullptr}; // scene node for rendering
        struct wl_listener map;     // fires when surface first gains a buffer
        struct wl_listener commit;  // fires on every client commit (needed for initial configure in wlroots 0.19)
        struct wl_listener destroy;
        struct wl_listener request_maximize;
        struct wl_listener request_fullscreen;
        struct wl_listener request_minimize;
        struct wl_listener request_move;
        struct wl_listener request_resize;
        WlrootsBackend* backend{nullptr};

        // Window states
        bool is_maximized{false};
        bool is_fullscreen{false};
        WindowStateMachine state_machine;

        // Saved geometry for restoring after maximize/fullscreen
        int32_t saved_x{50};
        int32_t saved_y{100};
        int32_t saved_width{800};
        int32_t saved_height{600};

        // Fade out on close
        bool is_closing{false};
        double opacity{1.0};
        std::chrono::steady_clock::time_point fade_start_time{};
        struct wl_event_source* fade_timer{nullptr};
    };
    std::vector<std::unique_ptr<ToplevelWrapper>> m_toplevels;
    std::vector<LayerSurfaceWrapper*> m_layer_surfaces;

    static void handle_new_layer_surface(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_layer_surface_listener);
        auto* layer_surface = static_cast<struct wlr_layer_surface_v1*>(data);

        log::info("[LayerShell] New layer surface: namespace='{}', layer={}",
            layer_surface->wl_namespace ? layer_surface->wl_namespace : "none",
            static_cast<int>(layer_surface->pending.layer));

        if (!layer_surface->output) {
            struct wlr_output* out = wlr_output_layout_output_at(self->m_output_layout, 0, 0);
            if (!out && !self->m_outputs.empty()) {
                out = self->m_outputs.front()->get_wlr_output();
            }
            if (out) {
                layer_surface->output = out;
            }
        }

        struct wlr_scene_tree* parent = self->m_scene_tree_top;
        switch (layer_surface->pending.layer) {
            case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
                parent = self->m_scene_tree_background;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
                parent = self->m_scene_tree_bottom;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
                parent = self->m_scene_tree_top;
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
                parent = self->m_scene_tree_overlay;
                break;
        }

        struct wlr_scene_layer_surface_v1* scene_layer =
            wlr_scene_layer_surface_v1_create(parent, layer_surface);

        auto* wrapper = new LayerSurfaceWrapper();
        wrapper->layer_surface = layer_surface;
        wrapper->scene_layer = scene_layer;
        wrapper->backend = self;
        self->m_layer_surfaces.push_back(wrapper);

        wrapper->destroy.notify = [](struct wl_listener* l, void* d) {
            LayerSurfaceWrapper* w = wl_container_of(l, w, destroy);
            
            // Remove from tracking
            WlrootsBackend* b = w->backend;
            
            // Defend against memory reuse bug: clear focus if this was the focused surface
            if (FocusManager::instance().keyboard_focus() == w->layer_surface->surface) {
                FocusManager::instance().set_keyboard_focus(nullptr);
            }

            auto it = std::find(b->m_layer_surfaces.begin(), b->m_layer_surfaces.end(), w);
            if (it != b->m_layer_surfaces.end()) {
                b->m_layer_surfaces.erase(it);
            }

            wl_list_remove(&w->destroy.link);
            wl_list_remove(&w->commit.link);
            delete w;
        };
        wl_signal_add(&layer_surface->events.destroy, &wrapper->destroy);

        wrapper->commit.notify = [](struct wl_listener* l, void* d) {
            LayerSurfaceWrapper* w = wl_container_of(l, w, commit);
            if (w->layer_surface->initial_commit || w->layer_surface->current.committed != 0) {
                if (!w->layer_surface->output) {
                    struct wlr_output* out = wlr_output_layout_output_at(w->backend->m_output_layout, 0, 0);
                    if (!out && !w->backend->m_outputs.empty()) {
                        out = w->backend->m_outputs.front()->get_wlr_output();
                    }
                    if (out) {
                        w->layer_surface->output = out;
                    }
                }
                struct wlr_box full_area = {0, 0, 0, 0};
                if (w->layer_surface->output) {
                    wlr_output_effective_resolution(w->layer_surface->output, &full_area.width, &full_area.height);
                }
                struct wlr_box usable_area = full_area;
                wlr_scene_layer_surface_v1_configure(w->scene_layer, &full_area, &usable_area);
            }

            log::debug("[LayerShell] commit.notify! namespace={}, actual_height={}",
                w->layer_surface->wl_namespace ? w->layer_surface->wl_namespace : "null",
                w->layer_surface->surface->current.height);

            // Auto-focus heuristic for the unified shell
            if (w->layer_surface->wl_namespace && std::string(w->layer_surface->wl_namespace) == "tinexus-shell") {
                if (w->layer_surface->surface->current.height > 100) {
                    if (w->backend->m_lock_surface == nullptr && FocusManager::instance().keyboard_focus() != w->layer_surface->surface) {
                        log::info("[LayerShell] Height > 100. Granting keyboard focus to Pulse.");
                        FocusManager::instance().set_keyboard_focus(w->layer_surface->surface);
                    }
                } else {
                    if (FocusManager::instance().keyboard_focus() == w->layer_surface->surface) {
                        FocusManager::instance().set_keyboard_focus(nullptr);
                        
                        // Restore focus to top toplevel
                        if (w->backend->m_lock_surface == nullptr) {
                            ToplevelWrapper* next_focus = nullptr;
                            if (!w->backend->m_toplevels.empty()) {
                                next_focus = w->backend->m_toplevels.back().get();
                            }
                            if (next_focus) {
                                w->backend->focus_toplevel(next_focus);
                            }
                        } else {
                            FocusManager::instance().set_keyboard_focus(w->backend->m_lock_surface);
                        }
                    }
                }
            }
        };
        wl_signal_add(&layer_surface->surface->events.commit, &wrapper->commit);
    }

    ToplevelWrapper* find_toplevel_from_node(struct wlr_scene_node* node) {
        if (!node) return nullptr;
        struct wlr_scene_node* current = node;
        while (current->parent != nullptr && current->parent != m_scene_tree_normal) {
            current = &current->parent->node;
        }
        for (const auto& w : m_toplevels) {
            if (w->scene_tree && &w->scene_tree->node == current) {
                return w.get();
            }
        }
        return nullptr;
    }

    void set_layer_surfaces_enabled(bool enabled) {
        int count = 0;
        for (auto* wrapper : m_layer_surfaces) {
            if (wrapper && wrapper->scene_layer && wrapper->scene_layer->tree) {
                wlr_scene_node_set_enabled(&wrapper->scene_layer->tree->node, enabled);
                count++;
            }
        }
        log::info("[LockState] {} {} layer-shell surfaces", 
            enabled ? "Restoring" : "Hiding", count);
    }


    void focus_toplevel(ToplevelWrapper* wrapper) {
        if (m_is_locked && m_lock_surface != nullptr) {
            // When locked, ONLY the lock surface pointer is permitted to receive focus
            if (wrapper != nullptr && wrapper->toplevel->base->surface != m_lock_surface) {
                return;
            }
        }
        if (wrapper != nullptr && wrapper->state_machine.state() == WindowState::Minimized) {
            wrapper->state_machine.request_restore();
            wlr_scene_node_set_enabled(&wrapper->scene_tree->node, true);
            log::info("[Window] Restored minimized window upon focus");
            std::string app_id_str = (wrapper->toplevel && wrapper->toplevel->app_id) ? wrapper->toplevel->app_id : "";
            if (TinexusServer::instance()) TinexusServer::instance()->notify_window_restored(app_id_str);
        }
        if (m_active_toplevel != nullptr && m_active_toplevel != wrapper) {
            wlr_xdg_toplevel_set_activated(m_active_toplevel->toplevel, false);
        }
        m_active_toplevel = wrapper;
        if (wrapper != nullptr) {
            log::info("[Window] Focus window app_id='{}' title='{}'",
                      wrapper->toplevel->app_id ? wrapper->toplevel->app_id : "unknown",
                      wrapper->toplevel->title ? wrapper->toplevel->title : "untitled");
            wlr_scene_node_set_enabled(&wrapper->scene_tree->node, true);
            wlr_scene_node_raise_to_top(&wrapper->scene_tree->node);
            wlr_xdg_toplevel_set_activated(wrapper->toplevel, true);
            FocusManager::instance().set_keyboard_focus(wrapper->toplevel->base->surface);

            std::string app_id_str = wrapper->toplevel->app_id ? get_canonical_app_id(wrapper->toplevel->app_id) : "";
            if (!app_id_str.empty() && TinexusServer::instance()) {
                TinexusServer::instance()->notify_app_focus_changed(app_id_str, 1);
            }
        } else {
            FocusManager::instance().set_keyboard_focus(nullptr);
        }
    }

    void focus_app(const std::string& app_id) noexcept override {
        std::string canonical = get_canonical_app_id(app_id);
        for (const auto& w : m_toplevels) {
            if (w->toplevel && w->toplevel->app_id &&
                (std::string(w->toplevel->app_id) == app_id ||
                 get_canonical_app_id(w->toplevel->app_id) == canonical)) {
                if (w->state_machine.state() == WindowState::Minimized) {
                    toplevel_set_minimized(w.get(), false);
                } else {
                    focus_toplevel(w.get());
                }
                break;
            }
        }
    }

    bool has_app(const std::string& app_id) const noexcept override {
        std::string canonical = get_canonical_app_id(app_id);
        for (const auto& w : m_toplevels) {
            if (w->toplevel && w->toplevel->app_id &&
                (std::string(w->toplevel->app_id) == app_id ||
                 get_canonical_app_id(w->toplevel->app_id) == canonical)) {
                return true;
            }
        }
        return false;
    }

    bool restore_window_by_app_id(const std::string& app_id) noexcept override {
        std::string canonical = get_canonical_app_id(app_id);
        for (const auto& w : m_toplevels) {
            if (w->toplevel && w->toplevel->app_id &&
                (std::string(w->toplevel->app_id) == app_id ||
                 get_canonical_app_id(w->toplevel->app_id) == canonical)) {
                if (w->state_machine.state() == WindowState::Minimized) {
                    toplevel_set_minimized(w.get(), false);
                } else {
                    focus_toplevel(w.get());
                }
                return true;
            }
        }
        return false;
    }

    bool minimize_window_by_app_id(const std::string& app_id) noexcept override {
        std::string canonical = get_canonical_app_id(app_id);
        for (const auto& w : m_toplevels) {
            if (w->toplevel && w->toplevel->app_id &&
                (std::string(w->toplevel->app_id) == app_id ||
                 get_canonical_app_id(w->toplevel->app_id) == canonical)) {
                if (w->state_machine.state() != WindowState::Minimized) {
                    toplevel_set_minimized(w.get(), true);
                }
                return true;
            }
        }
        return false;
    }

    void snap_active_window(SnapMode mode) noexcept override {
        if (m_active_toplevel) {
            toplevel_set_snap(m_active_toplevel, mode);
        }
    }

    void maximize_active_window() noexcept override {
        if (m_active_toplevel) {
            toplevel_set_maximized(m_active_toplevel, true);
        }
    }

    void restore_active_window() noexcept override {
        if (m_active_toplevel) {
            if (m_active_toplevel->state_machine.state() == WindowState::Maximized) {
                toplevel_set_maximized(m_active_toplevel, false);
            } else if (m_active_toplevel->state_machine.snap_mode() != SnapMode::None) {
                toplevel_restore_snap(m_active_toplevel);
            }
        }
    }

    bool toggle_launcher() noexcept override {
        for (const auto& w : m_toplevels) {
            if (w->toplevel->app_id && std::string(w->toplevel->app_id) == "tinexus-launcher") {
                log::info("[Backend] Closing existing launcher instance via XDG close");
                wlr_xdg_toplevel_send_close(w->toplevel);
                return true;
            }
        }
        return false;
    }

    using OutputWorkAreaInfo = WorkArea;

    struct wlr_output* get_output_for_toplevel(ToplevelWrapper* wrapper) const {
        if (!m_output_layout) return nullptr;

        int32_t wx = wrapper->scene_tree ? wrapper->scene_tree->node.x : 0;
        int32_t wy = wrapper->scene_tree ? wrapper->scene_tree->node.y : 0;
        int32_t ww = (wrapper->toplevel && wrapper->toplevel->base) ? wrapper->toplevel->base->current.geometry.width : 0;
        int32_t wh = (wrapper->toplevel && wrapper->toplevel->base) ? wrapper->toplevel->base->current.geometry.height : 0;
        if (ww <= 0) ww = 800;
        if (wh <= 0) wh = 600;

        double cx = wx + (ww / 2.0);
        double cy = wy + (wh / 2.0);

        struct wlr_output* out = wlr_output_layout_output_at(m_output_layout, cx, cy);
        if (!out) {
            out = wlr_output_layout_get_center_output(m_output_layout);
        }
        if (!out && !m_outputs.empty()) {
            out = m_outputs.front()->get_wlr_output();
        }
        return out;
    }

    OutputGeometry get_output_geometry(struct wlr_output* out) const {
        OutputGeometry geom{};
        if (!out || !m_output_layout) return geom;

        geom.output = out;
        if (out->name) {
            geom.connector = out->name;
        }

        struct wlr_box out_box{0, 0, 0, 0};
        wlr_output_layout_get_box(m_output_layout, out, &out_box);
        geom.global_x = out_box.x;
        geom.global_y = out_box.y;

        int32_t eff_w = 0, eff_h = 0;
        wlr_output_effective_resolution(out, &eff_w, &eff_h);
        if (eff_w <= 0) eff_w = out_box.width;
        if (eff_h <= 0) eff_h = out_box.height;

        geom.logical_width = eff_w;
        geom.logical_height = eff_h;
        geom.scale = out->scale > 0.0 ? out->scale : 1.0;
        geom.refresh_mhz = out->refresh > 0 ? static_cast<uint32_t>(out->refresh) : 60000;

        int32_t top_margin = 0;
        int32_t bottom_margin = 0;
        int32_t left_margin = 0;
        int32_t right_margin = 0;

        for (const auto* layer : m_layer_surfaces) {
            if (!layer || !layer->layer_surface) continue;

            // STRICT Multi-monitor isolation:
            // A layer surface exclusively belongs to its assigned output.
            struct wlr_output* layer_out = layer->layer_surface->output;
            if (layer_out == nullptr) {
                layer_out = !m_outputs.empty() ? m_outputs.front()->get_wlr_output() : nullptr;
            }
            if (layer_out != out) {
                continue; // Do not apply reservations across displays!
            }

            if (!layer->layer_surface->surface || !layer->layer_surface->surface->mapped) {
                continue;
            }

            int32_t zone = layer->layer_surface->current.exclusive_zone;
            if (zone <= 0) {
                continue; // Non-exclusive surface
            }

            uint32_t anchor = layer->layer_surface->current.anchor;
            const auto& margins = layer->layer_surface->current.margin;

            const bool has_top = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) != 0;
            const bool has_bottom = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) != 0;
            const bool has_left = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) != 0;
            const bool has_right = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) != 0;

            // Fullscreen layer surfaces spanning all 4 edges do not reserve edge margins
            if (has_top && has_bottom && has_left && has_right) {
                continue;
            }

            // Proper geometry-aware edge exclusive reservations
            if (has_top && !has_bottom) {
                top_margin = std::max(top_margin, zone + margins.top);
            } else if (has_bottom && !has_top) {
                bottom_margin = std::max(bottom_margin, zone + margins.bottom);
            } else if (has_left && !has_right) {
                left_margin = std::max(left_margin, zone + margins.left);
            } else if (has_right && !has_left) {
                right_margin = std::max(right_margin, zone + margins.right);
            }
        }

        geom.work_x = geom.global_x + left_margin;
        geom.work_y = geom.global_y + top_margin;
        geom.work_width = std::max(0, geom.logical_width - left_margin - right_margin);
        geom.work_height = std::max(0, geom.logical_height - top_margin - bottom_margin);
        geom.sync_compat();

        return geom;
    }

    WorkArea get_output_work_area(struct wlr_output* out) const {
        return get_output_geometry(out).work_area();
    }

    void toplevel_set_maximized(ToplevelWrapper* wrapper, bool maximize) {
        if (!wrapper || !wrapper->toplevel || !wrapper->scene_tree) return;

        if (maximize) {
            if (wrapper->state_machine.state() == WindowState::Maximized) {
                wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
                return;
            }

            struct wlr_output* out = get_output_for_toplevel(wrapper);
            if (!out) {
                log::warn("[Window] Cannot maximize window: no active output found");
                return;
            }

            WorkArea work_area = get_output_work_area(out);
            if (work_area.width <= 0 || work_area.height <= 0) {
                log::warn("[Window] Invalid output work area for maximize");
                return;
            }

            // Sync current scene position to floating geometry before maximize
            if (wrapper->state_machine.state() == WindowState::Normal &&
                wrapper->state_machine.snap_mode() == SnapMode::None) {
                int32_t cur_w = wrapper->toplevel->base->current.geometry.width;
                int32_t cur_h = wrapper->toplevel->base->current.geometry.height;
                if (cur_w <= 0) cur_w = 800;
                if (cur_h <= 0) cur_h = 600;
                wrapper->state_machine.update_floating_geometry(
                    wrapper->scene_tree->node.x,
                    wrapper->scene_tree->node.y,
                    cur_w,
                    cur_h
                );
            }

            WindowBox start_box{wrapper->scene_tree->node.x, wrapper->scene_tree->node.y,
                                wrapper->toplevel->base->current.geometry.width,
                                wrapper->toplevel->base->current.geometry.height};
            if (start_box.width <= 0) start_box.width = 800;
            if (start_box.height <= 0) start_box.height = 600;

            wrapper->state_machine.set_assigned_output(out);
            wrapper->state_machine.request_maximize(work_area);
            wrapper->is_maximized = true;

            // Update legacy saved fields for backward compatibility
            wrapper->saved_x = wrapper->state_machine.geometry().normal_geom.x;
            wrapper->saved_y = wrapper->state_machine.geometry().normal_geom.y;
            wrapper->saved_width = wrapper->state_machine.geometry().normal_geom.width;
            wrapper->saved_height = wrapper->state_machine.geometry().normal_geom.height;

            log::info("[Window] Maximize window to {}x{} at ({}, {}) on output '{}'",
                      work_area.width, work_area.height, work_area.x, work_area.y, out->name ? out->name : "unknown");

            wlr_xdg_toplevel_set_tiled(wrapper->toplevel, WLR_EDGE_NONE);
            wlr_xdg_toplevel_set_maximized(wrapper->toplevel, true);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, work_area.width, work_area.height);

            WindowBox target_box{work_area.x, work_area.y, work_area.width, work_area.height};
            uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
            AnimationManager::instance().start_animation({
                .window_id = anim_id,
                .target_handle = wrapper,
                .type = WindowAnimationType::Maximize,
                .curve = AnimationCurve::EaseDecelerate,
                .duration_sec = 0.160,
                .start_geom = start_box,
                .target_geom = target_box,
                .start_opacity = 1.0,
                .target_opacity = 1.0,
                .start_scale = 1.0,
                .target_scale = 1.0,
                .on_step = [wrapper](const WindowAnimation& a) {
                    if (wrapper && wrapper->scene_tree) {
                        wlr_scene_node_set_position(&wrapper->scene_tree->node, a.current_geom.x, a.current_geom.y);
                    }
                },
                .on_complete = [wrapper, target_box](const WindowAnimation&) {
                    if (wrapper && wrapper->scene_tree) {
                        wlr_scene_node_set_position(&wrapper->scene_tree->node, target_box.x, target_box.y);
                    }
                }
            });
        } else {
            if (wrapper->state_machine.state() != WindowState::Maximized) {
                wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
                return;
            }

            WindowBox start_box{wrapper->scene_tree->node.x, wrapper->scene_tree->node.y,
                                wrapper->toplevel->base->current.geometry.width,
                                wrapper->toplevel->base->current.geometry.height};
            if (start_box.width <= 0) start_box.width = 800;
            if (start_box.height <= 0) start_box.height = 600;

            wrapper->state_machine.request_restore();
            wrapper->is_maximized = false;

            const auto& norm = wrapper->state_machine.geometry().normal_geom;
            log::info("[Window] Restore maximized window to {}x{} at ({}, {})",
                      norm.width, norm.height, norm.x, norm.y);

            wlr_xdg_toplevel_set_tiled(wrapper->toplevel, WLR_EDGE_NONE);
            wlr_xdg_toplevel_set_maximized(wrapper->toplevel, false);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, norm.width, norm.height);

            WindowBox target_box = norm;
            uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
            AnimationManager::instance().start_animation({
                .window_id = anim_id,
                .target_handle = wrapper,
                .type = WindowAnimationType::Unmaximize,
                .curve = AnimationCurve::EaseDecelerate,
                .duration_sec = 0.160,
                .start_geom = start_box,
                .target_geom = target_box,
                .start_opacity = 1.0,
                .target_opacity = 1.0,
                .start_scale = 1.0,
                .target_scale = 1.0,
                .on_step = [wrapper](const WindowAnimation& a) {
                    if (wrapper && wrapper->scene_tree) {
                        wlr_scene_node_set_position(&wrapper->scene_tree->node, a.current_geom.x, a.current_geom.y);
                    }
                },
                .on_complete = [wrapper, target_box](const WindowAnimation&) {
                    if (wrapper && wrapper->scene_tree) {
                        wlr_scene_node_set_position(&wrapper->scene_tree->node, target_box.x, target_box.y);
                    }
                }
            });
        }
        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
    }

    void toplevel_set_snap(ToplevelWrapper* wrapper, SnapMode mode) {
        if (!wrapper || !wrapper->toplevel || !wrapper->scene_tree) return;
        if (mode == SnapMode::None) {
            toplevel_restore_snap(wrapper);
            return;
        }
        if (mode == SnapMode::Top) {
            toplevel_set_maximized(wrapper, true);
            return;
        }

        struct wlr_output* out = get_output_for_toplevel(wrapper);
        if (!out) return;
        WorkArea work_area = get_output_work_area(out);
        if (work_area.width <= 0 || work_area.height <= 0) return;

        if (wrapper->state_machine.state() == WindowState::Normal &&
            wrapper->state_machine.snap_mode() == SnapMode::None) {
            int32_t cur_w = wrapper->toplevel->base->current.geometry.width;
            int32_t cur_h = wrapper->toplevel->base->current.geometry.height;
            if (cur_w <= 0) cur_w = 800;
            if (cur_h <= 0) cur_h = 600;
            wrapper->state_machine.update_floating_geometry(
                wrapper->scene_tree->node.x,
                wrapper->scene_tree->node.y,
                cur_w,
                cur_h
            );
        }

        WindowBox start_box{wrapper->scene_tree->node.x, wrapper->scene_tree->node.y,
                            wrapper->toplevel->base->current.geometry.width,
                            wrapper->toplevel->base->current.geometry.height};
        if (start_box.width <= 0) start_box.width = 800;
        if (start_box.height <= 0) start_box.height = 600;

        wrapper->state_machine.set_assigned_output(out);
        wrapper->state_machine.request_snap(mode, work_area);

        WindowBox snap_box = wrapper->state_machine.geometry().snap_geom;
        log::info("[Window] Snap window (mode={}) to {}x{} at ({}, {}) on output '{}'",
                  static_cast<int>(mode), snap_box.width, snap_box.height, snap_box.x, snap_box.y,
                  out->name ? out->name : "unknown");

        uint32_t tiled_edges = WLR_EDGE_NONE;
        switch (mode) {
            case SnapMode::Left:
                tiled_edges = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
                break;
            case SnapMode::Right:
                tiled_edges = WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
                break;
            case SnapMode::TopLeft:
                tiled_edges = WLR_EDGE_LEFT | WLR_EDGE_TOP;
                break;
            case SnapMode::TopRight:
                tiled_edges = WLR_EDGE_RIGHT | WLR_EDGE_TOP;
                break;
            case SnapMode::BottomLeft:
                tiled_edges = WLR_EDGE_LEFT | WLR_EDGE_BOTTOM;
                break;
            case SnapMode::BottomRight:
                tiled_edges = WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM;
                break;
            default:
                break;
        }

        wlr_xdg_toplevel_set_maximized(wrapper->toplevel, false);
        wlr_xdg_toplevel_set_tiled(wrapper->toplevel, tiled_edges);
        wlr_xdg_toplevel_set_size(wrapper->toplevel, snap_box.width, snap_box.height);

        WindowBox target_box = snap_box;
        uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
        AnimationManager::instance().start_animation({
            .window_id = anim_id,
            .target_handle = wrapper,
            .type = WindowAnimationType::Snap,
            .curve = AnimationCurve::EaseDecelerate,
            .duration_sec = 0.150,
            .start_geom = start_box,
            .target_geom = target_box,
            .start_opacity = 1.0,
            .target_opacity = 1.0,
            .start_scale = 1.0,
            .target_scale = 1.0,
            .on_step = [wrapper](const WindowAnimation& a) {
                if (wrapper && wrapper->scene_tree) {
                    wlr_scene_node_set_position(&wrapper->scene_tree->node, a.current_geom.x, a.current_geom.y);
                }
            },
            .on_complete = [wrapper, target_box](const WindowAnimation&) {
                if (wrapper && wrapper->scene_tree) {
                    wlr_scene_node_set_position(&wrapper->scene_tree->node, target_box.x, target_box.y);
                }
            }
        });

        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
    }

    void toplevel_restore_snap(ToplevelWrapper* wrapper) {
        if (!wrapper || !wrapper->toplevel || !wrapper->scene_tree) return;
        if (wrapper->state_machine.snap_mode() == SnapMode::None) return;

        WindowBox start_box{wrapper->scene_tree->node.x, wrapper->scene_tree->node.y,
                            wrapper->toplevel->base->current.geometry.width,
                            wrapper->toplevel->base->current.geometry.height};
        if (start_box.width <= 0) start_box.width = 800;
        if (start_box.height <= 0) start_box.height = 600;

        wrapper->state_machine.request_restore();
        const auto& norm = wrapper->state_machine.geometry().normal_geom;
        log::info("[Window] Restore snapped window to {}x{} at ({}, {})",
                  norm.width, norm.height, norm.x, norm.y);

        wlr_xdg_toplevel_set_tiled(wrapper->toplevel, WLR_EDGE_NONE);
        wlr_xdg_toplevel_set_size(wrapper->toplevel, norm.width, norm.height);

        WindowBox target_box = norm;
        uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
        AnimationManager::instance().start_animation({
            .window_id = anim_id,
            .target_handle = wrapper,
            .type = WindowAnimationType::Snap,
            .curve = AnimationCurve::EaseDecelerate,
            .duration_sec = 0.150,
            .start_geom = start_box,
            .target_geom = target_box,
            .start_opacity = 1.0,
            .target_opacity = 1.0,
            .start_scale = 1.0,
            .target_scale = 1.0,
            .on_step = [wrapper](const WindowAnimation& a) {
                if (wrapper && wrapper->scene_tree) {
                    wlr_scene_node_set_position(&wrapper->scene_tree->node, a.current_geom.x, a.current_geom.y);
                }
            },
            .on_complete = [wrapper, target_box](const WindowAnimation&) {
                if (wrapper && wrapper->scene_tree) {
                    wlr_scene_node_set_position(&wrapper->scene_tree->node, target_box.x, target_box.y);
                }
            }
        });

        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
    }

    void toplevel_set_minimized(ToplevelWrapper* wrapper, bool minimize) {
        if (!wrapper || !wrapper->scene_tree) return;

        if (minimize) {
            if (wrapper->state_machine.state() == WindowState::Minimized) return;

            if (wrapper->state_machine.state() == WindowState::Normal &&
                wrapper->state_machine.snap_mode() == SnapMode::None) {
                int32_t cur_w = wrapper->toplevel->base->current.geometry.width;
                int32_t cur_h = wrapper->toplevel->base->current.geometry.height;
                if (cur_w <= 0) cur_w = 800;
                if (cur_h <= 0) cur_h = 600;
                wrapper->state_machine.update_floating_geometry(
                    wrapper->scene_tree->node.x,
                    wrapper->scene_tree->node.y,
                    cur_w,
                    cur_h
                );
            }

            wrapper->state_machine.request_minimize();
            log::info("[Window] Minimized toplevel — animating minimize");

            std::string app_id_str = (wrapper->toplevel && wrapper->toplevel->app_id) ?
                                     get_canonical_app_id(wrapper->toplevel->app_id) : "";
            if (TinexusServer::instance()) TinexusServer::instance()->notify_window_minimized(app_id_str);

            uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
            AnimationManager::instance().start_animation({
                .window_id = anim_id,
                .target_handle = wrapper,
                .type = WindowAnimationType::Minimize,
                .curve = AnimationCurve::EaseDecelerate,
                .duration_sec = 0.140,
                .start_opacity = 1.0,
                .target_opacity = 0.0,
                .start_scale = 1.0,
                .target_scale = 0.85,
                .on_step = [wrapper](const WindowAnimation& a) {
                    if (wrapper && wrapper->scene_tree) {
                        wrapper->opacity = a.current_opacity;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                },
                .on_complete = [wrapper](const WindowAnimation&) {
                    if (wrapper && wrapper->scene_tree) {
                        wlr_scene_node_set_enabled(&wrapper->scene_tree->node, false);
                        wrapper->opacity = 1.0;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                }
            });

            if (m_active_toplevel == wrapper) {
                m_active_toplevel = nullptr;
                ToplevelWrapper* next_focus = nullptr;
                for (auto it = m_toplevels.rbegin(); it != m_toplevels.rend(); ++it) {
                    if (it->get() != wrapper && it->get()->state_machine.state() != WindowState::Minimized) {
                        next_focus = it->get();
                        break;
                    }
                }
                focus_toplevel(next_focus);
            }
        } else {
            if (wrapper->state_machine.state() != WindowState::Minimized) return;

            wrapper->state_machine.request_restore();
            wlr_scene_node_set_enabled(&wrapper->scene_tree->node, true);
            log::info("[Window] Restored toplevel — animating restore");

            std::string app_id_str = (wrapper->toplevel && wrapper->toplevel->app_id) ?
                                     get_canonical_app_id(wrapper->toplevel->app_id) : "";
            if (TinexusServer::instance()) TinexusServer::instance()->notify_window_restored(app_id_str);

            uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
            wrapper->opacity = 0.0;
            wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);

            AnimationManager::instance().start_animation({
                .window_id = anim_id,
                .target_handle = wrapper,
                .type = WindowAnimationType::Restore,
                .curve = AnimationCurve::EaseDecelerate,
                .duration_sec = 0.140,
                .start_opacity = 0.0,
                .target_opacity = 1.0,
                .start_scale = 0.85,
                .target_scale = 1.0,
                .on_step = [wrapper](const WindowAnimation& a) {
                    if (wrapper && wrapper->scene_tree) {
                        wrapper->opacity = a.current_opacity;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                },
                .on_complete = [wrapper](const WindowAnimation&) {
                    if (wrapper && wrapper->scene_tree) {
                        wrapper->opacity = 1.0;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                }
            });

            focus_toplevel(wrapper);
        }
    }

    static void set_buffer_opacity(struct wlr_scene_buffer *buffer, int sx, int sy, void *user_data) {
        double opacity = *static_cast<double*>(user_data);
        wlr_scene_buffer_set_opacity(buffer, static_cast<float>(opacity));
    }

    void close_active_window() noexcept override {
        if (!m_active_toplevel || m_active_toplevel->is_closing) {
            return;
        }
        auto* wrapper = m_active_toplevel;
        wrapper->is_closing = true;
        wrapper->state_machine.mark_closing();
        wrapper->opacity = 1.0;
        uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);

        log::info("[Window] Initiating AnimationManager fade-out for window close");
        AnimationManager::instance().start_animation({
            .window_id = anim_id,
            .target_handle = wrapper,
            .type = WindowAnimationType::Close,
            .curve = AnimationCurve::EaseDecelerate,
            .duration_sec = 0.120,
            .start_opacity = 1.0,
            .target_opacity = 0.0,
            .on_step = [wrapper](const WindowAnimation& a) {
                if (wrapper && wrapper->scene_tree) {
                    wrapper->opacity = a.current_opacity;
                    wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                }
            },
            .on_complete = [wrapper](const WindowAnimation&) {
                if (wrapper && wrapper->toplevel) {
                    wlr_xdg_toplevel_send_close(wrapper->toplevel);
                }
            }
        });
    }

    SnapMode detect_snap_zone(double cx, double cy, struct wlr_output* out) const {
        if (!out) return SnapMode::None;
        OutputGeometry out_geom = get_output_geometry(out);
        if (out_geom.logical_width <= 0 || out_geom.logical_height <= 0) return SnapMode::None;

        const int edge_thresh = 16;
        const int corner_thresh = 64;

        if (cy <= out_geom.global_y + edge_thresh) {
            if (cx <= out_geom.global_x + corner_thresh) return SnapMode::TopLeft;
            if (cx >= out_geom.global_x + out_geom.logical_width - corner_thresh) return SnapMode::TopRight;
            return SnapMode::Top;
        }
        if (cy >= out_geom.global_y + out_geom.logical_height - edge_thresh) {
            if (cx <= out_geom.global_x + corner_thresh) return SnapMode::BottomLeft;
            if (cx >= out_geom.global_x + out_geom.logical_width - corner_thresh) return SnapMode::BottomRight;
            return SnapMode::None;
        }
        if (cx <= out_geom.global_x + edge_thresh) {
            if (cy <= out_geom.global_y + corner_thresh) return SnapMode::TopLeft;
            if (cy >= out_geom.global_y + out_geom.logical_height - corner_thresh) return SnapMode::BottomLeft;
            return SnapMode::Left;
        }
        if (cx >= out_geom.global_x + out_geom.logical_width - edge_thresh) {
            if (cy <= out_geom.global_y + corner_thresh) return SnapMode::TopRight;
            if (cy >= out_geom.global_y + out_geom.logical_height - corner_thresh) return SnapMode::BottomRight;
            return SnapMode::Right;
        }

        return SnapMode::None;
    }

    void toplevel_set_fullscreen(ToplevelWrapper* wrapper, bool fullscreen) {
        if (wrapper->is_fullscreen == fullscreen) {
            if (fullscreen && wrapper->scene_tree) {
                struct wlr_output* out = get_output_for_toplevel(wrapper);
                OutputGeometry out_geom = get_output_geometry(out);
                wlr_scene_node_set_position(&wrapper->scene_tree->node, out_geom.global_x, out_geom.global_y);
            }
            wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
            return;
        }
        wrapper->is_fullscreen = fullscreen;
        if (fullscreen) {
            if (!wrapper->is_maximized) {
                wrapper->saved_x = wrapper->scene_tree->node.x;
                wrapper->saved_y = wrapper->scene_tree->node.y;
                wrapper->saved_width = wrapper->toplevel->base->current.geometry.width;
                wrapper->saved_height = wrapper->toplevel->base->current.geometry.height;
                if (wrapper->saved_width <= 0) wrapper->saved_width = 800;
                if (wrapper->saved_height <= 0) wrapper->saved_height = 600;
                wrapper->state_machine.update_floating_geometry(wrapper->saved_x, wrapper->saved_y, wrapper->saved_width, wrapper->saved_height);
            }
            struct wlr_output* out = get_output_for_toplevel(wrapper);
            OutputGeometry out_geom = get_output_geometry(out);
            WindowBox output_box = out_geom.full_box();
            wrapper->state_machine.request_fullscreen(output_box);
            log::info("[Window] Fullscreen window to {}x{} at ({}, {})", output_box.width, output_box.height, output_box.x, output_box.y);
            wlr_scene_node_set_position(&wrapper->scene_tree->node, output_box.x, output_box.y);
            wlr_xdg_toplevel_set_fullscreen(wrapper->toplevel, true);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, output_box.width, output_box.height);
        } else {
            if (wrapper->is_maximized) {
                wrapper->is_maximized = false; // reset flag to trigger correct resize logic
                toplevel_set_maximized(wrapper, true);
            } else {
                wrapper->state_machine.request_restore();
                const auto& norm = wrapper->state_machine.geometry().normal_geom;
                log::info("[Window] Restore fullscreen window to {}x{} at ({}, {})", norm.width, norm.height, norm.x, norm.y);
                wlr_scene_node_set_position(&wrapper->scene_tree->node, norm.x, norm.y);
                wlr_xdg_toplevel_set_fullscreen(wrapper->toplevel, false);
                wlr_xdg_toplevel_set_size(wrapper->toplevel, norm.width, norm.height);
            }
        }
        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
    }

    void begin_interactive_move(ToplevelWrapper* wrapper) {
        m_cursor_mode = CursorMode::Move;
        m_grabbed_toplevel = wrapper;
        m_grab_x = m_cursor->x;
        m_grab_y = m_cursor->y;
        m_grab_geo_x = wrapper->scene_tree->node.x;
        m_grab_geo_y = wrapper->scene_tree->node.y;
        log::info("[Window] Started interactive move grab");
    }

    void begin_interactive_resize(ToplevelWrapper* wrapper, uint32_t edges) {
        if (!wrapper || !wrapper->toplevel || !wrapper->scene_tree) return;
        m_cursor_mode = CursorMode::Resize;
        m_grabbed_toplevel = wrapper;
        m_grab_edges = edges;
        m_grab_x = m_cursor->x;
        m_grab_y = m_cursor->y;
        m_grab_geo_x = wrapper->scene_tree->node.x;
        m_grab_geo_y = wrapper->scene_tree->node.y;
        m_grab_geobox = wrapper->toplevel->base->current.geometry;
        if (m_grab_geobox.width <= 0) {
            m_grab_geobox.width = 800;
        }
        if (m_grab_geobox.height <= 0) {
            m_grab_geobox.height = 600;
        }

        wlr_xdg_toplevel_set_resizing(wrapper->toplevel, true);
        const char* resize_cursor = wlr_xcursor_get_resize_name(static_cast<enum wlr_edges>(edges));
        if (resize_cursor) {
            wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, resize_cursor);
        }
        log::info("[Window] Started interactive resize grab (edges={})", edges);
    }

    static void handle_toplevel_request_move(struct wl_listener* listener, void* data) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, request_move);
        wrapper->backend->begin_interactive_move(wrapper);
    }

    static void handle_toplevel_request_resize(struct wl_listener* listener, void* data) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, request_resize);
        auto* event = static_cast<struct wlr_xdg_toplevel_resize_event*>(data);
        wrapper->backend->begin_interactive_resize(wrapper, event->edges);
    }

    static void handle_new_xdg_toplevel(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_xdg_surface_listener);
        auto* xdg_toplevel = static_cast<struct wlr_xdg_toplevel*>(data);

        log::info("[XDGShell] New XDG toplevel surface created");
        struct wlr_scene_tree* scene_tree = wlr_scene_xdg_surface_create(self->m_scene_tree_normal, xdg_toplevel->base);

        auto wrapper = std::make_unique<ToplevelWrapper>();
        wrapper->toplevel = xdg_toplevel;
        wrapper->scene_tree = scene_tree;
        wrapper->backend  = self;

        // Position window with cascade offset
        int32_t offset_x = 50 + static_cast<int32_t>((self->m_toplevels.size() % 5) * 30);
        int32_t offset_y = 100 + static_cast<int32_t>((self->m_toplevels.size() % 5) * 30);
        if (xdg_toplevel->app_id && (std::string(xdg_toplevel->app_id) == "lock" || std::string(xdg_toplevel->app_id) == "tinexus-lock")) {
            offset_x = 0;
            offset_y = 0;
            wrapper->saved_x = 0;
            wrapper->saved_y = 0;
        }
        wlr_scene_node_set_position(&scene_tree->node, offset_x, offset_y);

        // map fires when the surface first attaches a buffer (i.e. is ready to show)
        wrapper->map.notify = handle_toplevel_map;
        wl_signal_add(&xdg_toplevel->base->surface->events.map, &wrapper->map);

        // commit fires on client commits (needed to send configure on initial_commit)
        wrapper->commit.notify = handle_toplevel_commit;
        wl_signal_add(&xdg_toplevel->base->surface->events.commit, &wrapper->commit);

        // destroy — clean up our wrapper
        wrapper->destroy.notify = handle_toplevel_destroy;
        wl_signal_add(&xdg_toplevel->events.destroy, &wrapper->destroy);

        // State request listeners
        wrapper->request_maximize.notify = handle_toplevel_request_maximize;
        wl_signal_add(&xdg_toplevel->events.request_maximize, &wrapper->request_maximize);

        wrapper->request_fullscreen.notify = handle_toplevel_request_fullscreen;
        wl_signal_add(&xdg_toplevel->events.request_fullscreen, &wrapper->request_fullscreen);

        wrapper->request_minimize.notify = handle_toplevel_request_minimize;
        wl_signal_add(&xdg_toplevel->events.request_minimize, &wrapper->request_minimize);

        wrapper->request_move.notify = handle_toplevel_request_move;
        wl_signal_add(&xdg_toplevel->events.request_move, &wrapper->request_move);

        wrapper->request_resize.notify = handle_toplevel_request_resize;
        wl_signal_add(&xdg_toplevel->events.request_resize, &wrapper->request_resize);

        self->m_toplevels.push_back(std::move(wrapper));
    }

    static void handle_toplevel_map(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, map);
        struct wlr_surface* surface = wrapper->toplevel->base->surface;
        const char* app_id = wrapper->toplevel->app_id ? wrapper->toplevel->app_id : "";
        log::info("[XDGShell] Toplevel mapped — app_id='{}' surface={}",
                  app_id, static_cast<void*>(surface));

        // Initialize state machine with initial mapped position and geometry
        int32_t init_w = wrapper->toplevel->base->current.geometry.width;
        int32_t init_h = wrapper->toplevel->base->current.geometry.height;
        if (init_w <= 0) init_w = 800;
        if (init_h <= 0) init_h = 600;
        wrapper->state_machine.update_floating_geometry(
            wrapper->scene_tree->node.x,
            wrapper->scene_tree->node.y,
            init_w,
            init_h
        );
        struct wlr_output* out = wrapper->backend->get_output_for_toplevel(wrapper);
        if (out) {
            wrapper->state_machine.set_assigned_output(out);
        }

        std::string app_id_str = wrapper->toplevel->app_id ? get_canonical_app_id(wrapper->toplevel->app_id) : "";
        if (!app_id_str.empty() && TinexusServer::instance()) {
            TinexusServer::instance()->notify_app_started(app_id_str);
        }

        // If the lock screen just connected, track surface pointer and mark session locked
        if (std::string(app_id) == "lock" || std::string(app_id) == "tinexus-lock") {
            log::info("[XDGShell] Lock screen mapped — m_lock_surface={} session LOCKED", static_cast<void*>(surface));
            wrapper->backend->m_is_locked = true;
            wrapper->backend->m_lock_surface = surface;
            wrapper->backend->set_layer_surfaces_enabled(false);
            wrapper->saved_x = 0;
            wrapper->saved_y = 0;
            wlr_scene_node_set_position(&wrapper->scene_tree->node, 0, 0);
            wrapper->backend->toplevel_set_fullscreen(wrapper, true);
        } else if (wrapper->is_fullscreen) {
            wlr_scene_node_set_position(&wrapper->scene_tree->node, 0, 0);
        } else if (wrapper->toplevel && wrapper->scene_tree && !wrapper->backend->m_is_locked) {
            uint64_t anim_id = reinterpret_cast<uint64_t>(wrapper);
            wrapper->opacity = 0.0;
            wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);

            AnimationManager::instance().start_animation({
                .window_id = anim_id,
                .target_handle = wrapper,
                .type = WindowAnimationType::Open,
                .curve = AnimationCurve::EaseDecelerate,
                .duration_sec = 0.120,
                .start_opacity = 0.0,
                .target_opacity = 1.0,
                .on_step = [wrapper](const WindowAnimation& a) {
                    if (wrapper && wrapper->scene_tree) {
                        wrapper->opacity = a.current_opacity;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                },
                .on_complete = [wrapper](const WindowAnimation&) {
                    if (wrapper && wrapper->scene_tree) {
                        wrapper->opacity = 1.0;
                        wlr_scene_node_for_each_buffer(&wrapper->scene_tree->node, set_buffer_opacity, &wrapper->opacity);
                    }
                }
            });
        }

        wrapper->backend->focus_toplevel(wrapper);
    }


    static void handle_toplevel_commit(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, commit);
        struct wlr_xdg_toplevel* toplevel = wrapper->toplevel;
        if (toplevel->base->initial_commit) {
            log::info("[XDGShell] Initial commit for toplevel — scheduling initial configure");
            wlr_xdg_surface_schedule_configure(toplevel->base);
        }
    }

    static void handle_toplevel_request_maximize(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, request_maximize);
        struct wlr_xdg_toplevel* toplevel = wrapper->toplevel;
        log::info("[XDGShell] Request maximize state={}", toplevel->requested.maximized);
        wrapper->backend->toplevel_set_maximized(wrapper, toplevel->requested.maximized);
    }

    static void handle_toplevel_request_fullscreen(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, request_fullscreen);
        struct wlr_xdg_toplevel* toplevel = wrapper->toplevel;
        log::info("[XDGShell] Request fullscreen state={}", toplevel->requested.fullscreen);
        wrapper->backend->toplevel_set_fullscreen(wrapper, toplevel->requested.fullscreen);
    }

    static void handle_toplevel_request_minimize(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, request_minimize);
        struct wlr_xdg_toplevel* toplevel = wrapper->toplevel;
        log::info("[XDGShell] Request minimize for toplevel");
        wrapper->backend->toplevel_set_minimized(wrapper, true);
        wlr_xdg_surface_schedule_configure(toplevel->base);
    }

    static void handle_toplevel_destroy(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, destroy);
        WlrootsBackend* backend = wrapper->backend;
        struct wlr_surface* surface = (wrapper->toplevel && wrapper->toplevel->base) ? wrapper->toplevel->base->surface : nullptr;

        log::info("[XDGShell] handle_toplevel_destroy for wrapper={}", static_cast<void*>(wrapper));

        // 0. Cancel active animations immediately to prevent any callback or dereference
        AnimationManager::instance().cancel_animation(reinterpret_cast<uint64_t>(wrapper));

        // 1. Cancel/disarm timers (F-03 safety)
        if (wrapper->fade_timer != nullptr) {
            wl_event_source_remove(wrapper->fade_timer);
            wrapper->fade_timer = nullptr;
            log::info("[XDGShell] Disarmed fade_timer during toplevel destruction");
        }

        // 2. Clear/release grabs (F-03 safety)
        if (backend->m_grabbed_toplevel == wrapper) {
            backend->m_grabbed_toplevel = nullptr;
            backend->m_cursor_mode = CursorMode::Passthrough;
            backend->m_pending_snap = SnapMode::None;
            backend->m_grab_edges = 0;
            log::info("[XDGShell] Released cursor grab during toplevel destruction");
        }

        // 3. Cancel/remove references to the real wrapper
        if (backend->m_active_toplevel == wrapper) {
            backend->m_active_toplevel = nullptr;
        }
        wrapper->state_machine.mark_closing();

        const bool was_lock = (surface && surface == backend->m_lock_surface);
        if (was_lock) {
            log::info("[XDGShell] Lock screen destroyed — marking session UNLOCKED atomically");
            backend->m_is_locked = false;
            backend->m_lock_surface = nullptr;
            backend->set_layer_surfaces_enabled(true);
        }

        // 4. Remove compositor registries / focus references
        if (surface) {
            if (FocusManager::instance().keyboard_focus() == surface) {
                FocusManager::instance().set_keyboard_focus(nullptr);
            }
            if (FocusManager::instance().pointer_surface() == surface) {
                FocusManager::instance().update_pointer_focus(PickResult{nullptr, 0.0, 0.0}, 0);
            }
        }

        // Remove wl_listeners safely and idempotently
        if (wrapper->map.link.next != nullptr) { wl_list_remove(&wrapper->map.link); wrapper->map.link.next = nullptr; }
        if (wrapper->commit.link.next != nullptr) { wl_list_remove(&wrapper->commit.link); wrapper->commit.link.next = nullptr; }
        if (wrapper->destroy.link.next != nullptr) { wl_list_remove(&wrapper->destroy.link); wrapper->destroy.link.next = nullptr; }
        if (wrapper->request_maximize.link.next != nullptr) { wl_list_remove(&wrapper->request_maximize.link); wrapper->request_maximize.link.next = nullptr; }
        if (wrapper->request_fullscreen.link.next != nullptr) { wl_list_remove(&wrapper->request_fullscreen.link); wrapper->request_fullscreen.link.next = nullptr; }
        if (wrapper->request_minimize.link.next != nullptr) { wl_list_remove(&wrapper->request_minimize.link); wrapper->request_minimize.link.next = nullptr; }
        if (wrapper->request_move.link.next != nullptr) { wl_list_remove(&wrapper->request_move.link); wrapper->request_move.link.next = nullptr; }
        if (wrapper->request_resize.link.next != nullptr) { wl_list_remove(&wrapper->request_resize.link); wrapper->request_resize.link.next = nullptr; }

        // 5. Remove from list (destroys wrapper and its scene resources via unique_ptr)
        std::string dead_app_id = (wrapper->toplevel && wrapper->toplevel->app_id) ?
                                  get_canonical_app_id(wrapper->toplevel->app_id) : "";
        for (auto it = backend->m_toplevels.begin(); it != backend->m_toplevels.end(); ++it) {
            if (it->get() == wrapper) {
                backend->m_toplevels.erase(it);
                break;
            }
        }

        if (!dead_app_id.empty()) {
            bool has_remaining = false;
            for (const auto& other : backend->m_toplevels) {
                if (other->toplevel && other->toplevel->app_id) {
                    if (get_canonical_app_id(other->toplevel->app_id) == dead_app_id) {
                        has_remaining = true;
                        break;
                    }
                }
            }
            if (!has_remaining && TinexusServer::instance()) {
                TinexusServer::instance()->notify_app_closed(dead_app_id);
            }
        }

        // 6. Restore focus to top available toplevel immediately without gap
        if (backend->m_active_toplevel == nullptr) {
            ToplevelWrapper* next_focus = nullptr;
            for (auto it = backend->m_toplevels.rbegin(); it != backend->m_toplevels.rend(); ++it) {
                if (it->get()->state_machine.state() != WindowState::Minimized &&
                    it->get()->state_machine.state() != WindowState::Closing) {
                    next_focus = it->get();
                    break;
                }
            }
            backend->focus_toplevel(next_focus);
        }
    }

    static void handle_new_output(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_output_listener);
        auto* wlr_out = static_cast<struct wlr_output*>(data);

        auto output = std::make_unique<TinexusOutput>(wlr_out, self->m_wlr_allocator, self->m_wlr_renderer, self->m_scene);
        if (output->initialize()) {
            wlr_output_layout_add_auto(self->m_output_layout, wlr_out);

            // CRITICAL: Register this output with the scene graph.
            struct wlr_scene_output* scene_out = wlr_scene_output_create(self->m_scene, wlr_out);
            if (!scene_out) {
                log::error("[Backend] Failed to create scene output for '{}'", wlr_out->name);
            } else {
                log::info("[Backend] Scene output registered for '{}'", wlr_out->name);
            }

            // Milestone 1: Draw a static blue background #0F172A
            float color[4] = {0.059f, 0.09f, 0.165f, 1.0f}; // roughly #0F172A
            struct wlr_scene_rect* bg_rect = wlr_scene_rect_create(self->m_scene_tree_background, 10000, 10000, color);
            wlr_scene_node_set_position(&bg_rect->node, 0, 0);

            self->m_outputs.push_back(std::move(output));

            // Reconfigure any layer surfaces with authoritative output geometry
            for (auto* layer_w : self->m_layer_surfaces) {
                if (!layer_w || !layer_w->layer_surface) continue;
                if (!layer_w->layer_surface->output) {
                    layer_w->layer_surface->output = wlr_out;
                }
                struct wlr_box full_area = {0, 0, 0, 0};
                wlr_output_effective_resolution(layer_w->layer_surface->output, &full_area.width, &full_area.height);
                struct wlr_box usable_area = full_area;
                wlr_scene_layer_surface_v1_configure(layer_w->scene_layer, &full_area, &usable_area);
            }
        } else {
            log::error("[Backend] Failed to initialize TinexusOutput for '{}'", wlr_out->name);
        }
    }

    static void handle_new_input(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_input_listener);
        auto* device = static_cast<struct wlr_input_device*>(data);

        log::info("[Input] New input device detected: '{}' (type={})", device->name, static_cast<int>(device->type));

        switch (device->type) {
            case WLR_INPUT_DEVICE_KEYBOARD:
                log::info("[Input] Detected Keyboard: {}", device->name);
                self->setup_keyboard(device);
                break;
            case WLR_INPUT_DEVICE_POINTER:
                log::info("[Input] Detected Pointer: {}", device->name);
                self->setup_pointer(device);
                break;
            case WLR_INPUT_DEVICE_TABLET:
                log::info("[Input] Detected Tablet (Pointer): {}", device->name);
                self->setup_pointer(device);
                break;
            case WLR_INPUT_DEVICE_TOUCH:
                log::info("[Input] Detected Touchscreen: {}", device->name);
                self->setup_pointer(device);
                break;
            case WLR_INPUT_DEVICE_TABLET_PAD:
                log::info("[Input] Detected Tablet Pad: {}", device->name);
                break;
            case WLR_INPUT_DEVICE_SWITCH:
                log::info("[Input] Detected Switch: {}", device->name);
                break;
            default:
                log::info("[Input] Detected Unknown Device: {}", device->name);
                wlr_cursor_attach_input_device(self->m_cursor, device);
                break;
        }

        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!self->m_keyboards.empty()) {
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        }
        wlr_seat_set_capabilities(self->m_seat, caps);
    }

    void setup_keyboard(struct wlr_input_device* device) {
        auto* keyboard = wlr_keyboard_from_input_device(device);
        auto wrapper = std::make_unique<KeyboardWrapper>();
        wrapper->keyboard = keyboard;
        wrapper->backend = this;

        struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap* keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

        wlr_keyboard_set_keymap(keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        wlr_keyboard_set_repeat_info(keyboard, 25, 600);

        wrapper->modifiers.notify = handle_keyboard_modifiers;
        wl_signal_add(&keyboard->events.modifiers, &wrapper->modifiers);

        wrapper->key.notify = handle_keyboard_key;
        wl_signal_add(&keyboard->events.key, &wrapper->key);

        wrapper->destroy.notify = handle_keyboard_destroy;
        wl_signal_add(&device->events.destroy, &wrapper->destroy);

        wlr_seat_set_keyboard(m_seat, keyboard);
        m_keyboards.push_back(std::move(wrapper));
    }

    static void handle_keyboard_modifiers(struct wl_listener* listener, void* data) {
        KeyboardWrapper* wrapper = wl_container_of(listener, wrapper, modifiers);
        SeatManager::instance().notify_keyboard_modifiers(wrapper->keyboard);
    }

    static void handle_keyboard_key(struct wl_listener* listener, void* data) {
        KeyboardWrapper* wrapper = wl_container_of(listener, wrapper, key);
        auto* event = static_cast<struct wlr_keyboard_key_event*>(data);

        // XKB keycode = evdev keycode + 8
        uint32_t keycode = event->keycode + 8;
        bool is_pressed = (event->state == WL_KEYBOARD_KEY_STATE_PRESSED);

        // Resolve XKB keysym for diagnostics and shortcut detection
        xkb_keysym_t primary_sym = XKB_KEY_NoSymbol;
        if (wrapper->keyboard->xkb_state) {
            const xkb_keysym_t* syms;
            int nsyms = xkb_state_key_get_syms(
                wrapper->keyboard->xkb_state, keycode, &syms);
            if (nsyms > 0) {
                primary_sym = syms[0];
                log::info("[Keyboard] Key sym={} state={}",
                          primary_sym, static_cast<uint32_t>(event->state));
            }
        } else {
            log::warn("[Keyboard] xkb_state is NULL! Cannot resolve keycode {}", keycode);
        }

        // ── Global shortcut interception (Ctrl+K → launcher) ──────────────────
        // SECURITY: when locked, ALL shortcuts are suppressed — keys go to lock client only
        if (!wrapper->backend->m_is_locked) {
            uint32_t wlr_mods = wlr_keyboard_get_modifiers(wrapper->keyboard);
            if (ShortcutEngine::instance().process_key_event(wlr_mods, event->keycode, is_pressed, primary_sym)) {
                log::info("[Keyboard] Global shortcut intercepted — swallowing key event.");
                return;
            }
        }

        // Forward normal key to focused client via seat
        SeatManager::instance().notify_keyboard_key(
            wrapper->keyboard,
            event->time_msec,
            event->keycode,
            static_cast<uint32_t>(event->state));
    }

    void setup_pointer(struct wlr_input_device* device) {
        auto wrapper = std::make_unique<PointerWrapper>();
        wrapper->device = device;
        wrapper->backend = this;

        wrapper->destroy.notify = handle_pointer_destroy;
        wl_signal_add(&device->events.destroy, &wrapper->destroy);

        wlr_cursor_attach_input_device(m_cursor, device);
        m_pointers.push_back(std::move(wrapper));
    }

    static void handle_pointer_destroy(struct wl_listener* listener, void* data) {
        PointerWrapper* wrapper = wl_container_of(listener, wrapper, destroy);
        WlrootsBackend* self = wrapper->backend;

        log::info("[Input] Pointer device destroyed: '{}'", wrapper->device->name);
        wlr_cursor_detach_input_device(self->m_cursor, wrapper->device);
        wl_list_remove(&wrapper->destroy.link);

        for (auto it = self->m_pointers.begin(); it != self->m_pointers.end(); ++it) {
            if (it->get() == wrapper) {
                self->m_pointers.erase(it);
                break;
            }
        }
    }

    static void handle_keyboard_destroy(struct wl_listener* listener, void* data) {
        KeyboardWrapper* wrapper = wl_container_of(listener, wrapper, destroy);
        WlrootsBackend* self = wrapper->backend;

        wl_list_remove(&wrapper->modifiers.link);
        wl_list_remove(&wrapper->key.link);
        wl_list_remove(&wrapper->destroy.link);

        if (self) {
            for (auto it = self->m_keyboards.begin(); it != self->m_keyboards.end(); ++it) {
                if (it->get() == wrapper) {
                    self->m_keyboards.erase(it);
                    break;
                }
            }
        }
    }

    static void handle_cursor_motion(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_motion_listener);
        auto* event = static_cast<struct wlr_pointer_motion_event*>(data);
        wlr_cursor_move(self->m_cursor, &event->pointer->base, event->delta_x, event->delta_y);
        self->process_cursor_motion(event->time_msec);
    }

    static void handle_cursor_motion_absolute(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_motion_absolute_listener);
        auto* event = static_cast<struct wlr_pointer_motion_absolute_event*>(data);
        wlr_cursor_warp_absolute(self->m_cursor, &event->pointer->base, event->x, event->y);
        self->process_cursor_motion(event->time_msec);
    }

    void process_cursor_motion(uint32_t time) {
        if (m_cursor_mode == CursorMode::Passthrough) {
            wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
        }

        if (m_cursor_mode == CursorMode::Move && m_grabbed_toplevel != nullptr) {
            // Drag > 8px triggers un-snap / un-maximize
            double drag_dist = std::hypot(m_cursor->x - m_grab_x, m_cursor->y - m_grab_y);
            if (drag_dist > 8.0) {
                if (m_grabbed_toplevel->state_machine.state() == WindowState::Maximized) {
                    toplevel_set_maximized(m_grabbed_toplevel, false);
                    const auto& norm = m_grabbed_toplevel->state_machine.geometry().normal_geom;
                    m_grab_geo_x = static_cast<int>(m_cursor->x - (norm.width / 2.0));
                    m_grab_geo_y = static_cast<int>(m_cursor->y - 15);
                    m_grab_x = m_cursor->x;
                    m_grab_y = m_cursor->y;
                } else if (m_grabbed_toplevel->state_machine.snap_mode() != SnapMode::None) {
                    toplevel_restore_snap(m_grabbed_toplevel);
                    const auto& norm = m_grabbed_toplevel->state_machine.geometry().normal_geom;
                    m_grab_geo_x = static_cast<int>(m_cursor->x - (norm.width / 2.0));
                    m_grab_geo_y = static_cast<int>(m_cursor->y - 15);
                    m_grab_x = m_cursor->x;
                    m_grab_y = m_cursor->y;
                }
            }

            int new_x = m_grab_geo_x + static_cast<int>(m_cursor->x - m_grab_x);
            int new_y = m_grab_geo_y + static_cast<int>(m_cursor->y - m_grab_y);
            wlr_scene_node_set_position(&m_grabbed_toplevel->scene_tree->node, new_x, new_y);
            if (m_grabbed_toplevel->state_machine.state() == WindowState::Normal &&
                m_grabbed_toplevel->state_machine.snap_mode() == SnapMode::None) {
                m_grabbed_toplevel->state_machine.update_floating_geometry(
                    new_x,
                    new_y,
                    m_grabbed_toplevel->state_machine.geometry().current_geom.width,
                    m_grabbed_toplevel->state_machine.geometry().current_geom.height
                );
            }

            struct wlr_output* out = wlr_output_layout_output_at(m_output_layout, m_cursor->x, m_cursor->y);
            if (!out) out = get_output_for_toplevel(m_grabbed_toplevel);
            m_pending_snap = detect_snap_zone(m_cursor->x, m_cursor->y, out);
            return;
        } else if (m_cursor_mode == CursorMode::Resize && m_grabbed_toplevel != nullptr) {
            double dx = m_cursor->x - m_grab_x;
            double dy = m_cursor->y - m_grab_y;

            int32_t init_x = m_grab_geo_x;
            int32_t init_y = m_grab_geo_y;
            int32_t init_w = m_grab_geobox.width;
            int32_t init_h = m_grab_geobox.height;

            int32_t new_x = init_x;
            int32_t new_y = init_y;
            int32_t new_w = init_w;
            int32_t new_h = init_h;

            int32_t min_w = std::max(100, m_grabbed_toplevel->toplevel->current.min_width);
            int32_t min_h = std::max(100, m_grabbed_toplevel->toplevel->current.min_height);
            int32_t max_w = m_grabbed_toplevel->toplevel->current.max_width > 0 ?
                            m_grabbed_toplevel->toplevel->current.max_width : 32767;
            int32_t max_h = m_grabbed_toplevel->toplevel->current.max_height > 0 ?
                            m_grabbed_toplevel->toplevel->current.max_height : 32767;

            if (m_grab_edges & WLR_EDGE_RIGHT) {
                new_w = std::clamp(init_w + static_cast<int32_t>(dx), min_w, max_w);
            } else if (m_grab_edges & WLR_EDGE_LEFT) {
                new_w = std::clamp(init_w - static_cast<int32_t>(dx), min_w, max_w);
                new_x = init_x + (init_w - new_w);
            }

            if (m_grab_edges & WLR_EDGE_BOTTOM) {
                new_h = std::clamp(init_h + static_cast<int32_t>(dy), min_h, max_h);
            } else if (m_grab_edges & WLR_EDGE_TOP) {
                new_h = std::clamp(init_h - static_cast<int32_t>(dy), min_h, max_h);
                new_y = init_y + (init_h - new_h);
            }

            wlr_scene_node_set_position(&m_grabbed_toplevel->scene_tree->node, new_x, new_y);
            wlr_xdg_toplevel_set_size(m_grabbed_toplevel->toplevel, new_w, new_h);
            if (m_grabbed_toplevel->state_machine.state() == WindowState::Normal &&
                m_grabbed_toplevel->state_machine.snap_mode() == SnapMode::None) {
                m_grabbed_toplevel->state_machine.update_floating_geometry(new_x, new_y, new_w, new_h);
            }
            wlr_xdg_surface_schedule_configure(m_grabbed_toplevel->toplevel->base);
            return;
        }

        // [3D.3] Scene-graph hit-test → FocusManager handles everything:
        //        wlr_scene_node_at  →  notify_enter / notify_motion / clear_focus
        const PickResult pick = FocusManager::instance().pick_surface(
            m_cursor->x, m_cursor->y);
        FocusManager::instance().update_pointer_focus(pick, time);

        // [3D.4] Update internal position tracker (lightweight)
        CursorManager::instance().update_position(m_cursor->x, m_cursor->y);
    }

    static void handle_cursor_button(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_button_listener);
        auto* event = static_cast<struct wlr_pointer_button_event*>(data);

        // Click-to-focus: on button press, find window from clicked scene node, focus & raise it
        if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
            double sx{0.0}, sy{0.0};
            struct wlr_scene_node* node = wlr_scene_node_at(
                &self->m_scene->tree.node, self->m_cursor->x, self->m_cursor->y, &sx, &sy);
            if (node) {
                ToplevelWrapper* clicked_wrapper = self->find_toplevel_from_node(node);
                if (clicked_wrapper != nullptr) {
                    const char* app_name = (clicked_wrapper->toplevel && clicked_wrapper->toplevel->app_id)
                        ? clicked_wrapper->toplevel->app_id : "unknown";
                    log::info("[Window] Click at ({:.1f}, {:.1f}) focused toplevel '{}' ({})",
                              self->m_cursor->x, self->m_cursor->y,
                              app_name, static_cast<void*>(clicked_wrapper));
                    self->focus_toplevel(clicked_wrapper);
                } else if (self->m_cursor->y <= 60.0) {
                    log::debug("[Window] Click at ({:.1f}, {:.1f}) hit non-toplevel node {}",
                               self->m_cursor->x, self->m_cursor->y, static_cast<void*>(node));
                }
            }
        } else if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
            if (self->m_cursor_mode == CursorMode::Move && self->m_grabbed_toplevel != nullptr) {
                if (self->m_pending_snap == SnapMode::Top) {
                    self->toplevel_set_maximized(self->m_grabbed_toplevel, true);
                } else if (self->m_pending_snap != SnapMode::None) {
                    self->toplevel_set_snap(self->m_grabbed_toplevel, self->m_pending_snap);
                }
                self->m_pending_snap = SnapMode::None;
            } else if (self->m_cursor_mode == CursorMode::Resize && self->m_grabbed_toplevel != nullptr) {
                wlr_xdg_toplevel_set_resizing(self->m_grabbed_toplevel->toplevel, false);
                wlr_xdg_surface_schedule_configure(self->m_grabbed_toplevel->toplevel->base);
            }

            if (self->m_cursor_mode != CursorMode::Passthrough) {
                log::info("[Window] Ended grab");
                self->m_cursor_mode = CursorMode::Passthrough;
                self->m_grabbed_toplevel = nullptr;
                self->m_grab_edges = 0;
                wlr_cursor_set_xcursor(self->m_cursor, self->m_cursor_mgr, "default");
            }
        }

        SeatManager::instance().notify_button(
            event->time_msec, event->button,
            static_cast<uint32_t>(event->state));
    }

    static void handle_cursor_axis(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_axis_listener);
        auto* event = static_cast<struct wlr_pointer_axis_event*>(data);
        self->process_cursor_motion(event->time_msec);
        SeatManager::instance().notify_axis(
            event->time_msec,
            static_cast<uint32_t>(event->orientation),
            event->delta,
            event->delta_discrete,
            static_cast<uint32_t>(event->source),
            static_cast<uint32_t>(event->relative_direction));
    }

    static void handle_cursor_frame(struct wl_listener* listener, void* data) {
        SeatManager::instance().notify_frame();
    }
};

std::unique_ptr<Backend> create_wlroots_backend(struct wl_display* display) {
    return std::make_unique<WlrootsBackend>(display);
}

} // namespace tinexus::comp
