#include "comp/backend/backend.hpp"
#include "common/logger.hpp"
#include "comp/output/output.hpp"
#include <vector>
#include <memory>

extern "C" {
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/render/wlr_renderer.h>
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
#define namespace wl_namespace
#include <wlr/types/wlr_layer_shell_v1.h>
#define static
#include <wlr/types/wlr_scene.h>
#undef static
#include <wlr/types/wlr_xdg_shell.h>
#undef namespace
#include <xkbcommon/xkbcommon.h>
}

#include "comp/input/seat_manager.hpp"
#include "comp/input/shortcut_engine.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/focus/focus_manager.hpp"
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>

namespace tinexus::comp {

class WlrootsBackend : public Backend {
public:
    explicit WlrootsBackend(struct wl_display* display) : m_display(display) {}

    ~WlrootsBackend() override {
        shutdown();
    }

    bool initialize() override {
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

        return true;
    }

    bool start() override {
        log::info("[Backend] Starting wlroots backend...");
        if (!wlr_backend_start(m_wlr_backend)) {
            log::error("[Backend] Failed to start wlroots backend.");
            return false;
        }
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

    // Session lock state — when true, ALL global shortcuts are suppressed
    // and keyboard input goes exclusively to the lock client
    bool m_is_locked{false};

    // Tracks the Wayland socket name so child processes inherit WAYLAND_DISPLAY
    std::string m_wayland_socket{};


    struct LayerSurfaceWrapper {
        struct wlr_layer_surface_v1* layer_surface{nullptr};
        struct wlr_scene_layer_surface_v1* scene_layer{nullptr};
        struct wl_listener destroy;
        struct wl_listener commit;
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
        WlrootsBackend* backend{nullptr};

        // Window states
        bool is_maximized{false};
        bool is_fullscreen{false};

        // Saved geometry for restoring after maximize/fullscreen
        int32_t saved_x{50};
        int32_t saved_y{100};
        int32_t saved_width{800};
        int32_t saved_height{600};
    };
    std::vector<std::unique_ptr<ToplevelWrapper>> m_toplevels;

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

        wrapper->destroy.notify = [](struct wl_listener* l, void* d) {
            LayerSurfaceWrapper* w = wl_container_of(l, w, destroy);
            wl_list_remove(&w->destroy.link);
            wl_list_remove(&w->commit.link);
            delete w;
        };
        wl_signal_add(&layer_surface->events.destroy, &wrapper->destroy);

        wrapper->commit.notify = [](struct wl_listener* l, void* d) {
            LayerSurfaceWrapper* w = wl_container_of(l, w, commit);
            struct wlr_box full_area = {0, 0, 0, 0};
            if (w->layer_surface->output) {
                wlr_output_effective_resolution(w->layer_surface->output, &full_area.width, &full_area.height);
            }
            struct wlr_box usable_area = full_area;
            wlr_scene_layer_surface_v1_configure(w->scene_layer, &full_area, &usable_area);
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

    void focus_toplevel(ToplevelWrapper* wrapper) {
        if (m_is_locked && m_lock_surface != nullptr) {
            // When locked, ONLY the lock surface pointer is permitted to receive focus
            if (wrapper != nullptr && wrapper->toplevel->base->surface != m_lock_surface) {
                return;
            }
        }
        if (m_active_toplevel == wrapper) {
            return;
        }
        if (m_active_toplevel != nullptr) {
            wlr_xdg_toplevel_set_activated(m_active_toplevel->toplevel, false);
        }
        m_active_toplevel = wrapper;
        if (wrapper != nullptr) {
            log::info("[Window] Focus window app_id='{}' title='{}'",
                      wrapper->toplevel->app_id ? wrapper->toplevel->app_id : "unknown",
                      wrapper->toplevel->title ? wrapper->toplevel->title : "untitled");
            wlr_xdg_toplevel_set_activated(wrapper->toplevel, true);
            wlr_scene_node_raise_to_top(&wrapper->scene_tree->node);
            FocusManager::instance().set_keyboard_focus(wrapper->toplevel->base->surface);
        } else {
            FocusManager::instance().set_keyboard_focus(nullptr);
        }
    }

    void toplevel_set_maximized(ToplevelWrapper* wrapper, bool maximize) {
        if (wrapper->is_maximized == maximize) {
            wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
            return;
        }
        wrapper->is_maximized = maximize;
        if (maximize) {
            if (!wrapper->is_fullscreen) {
                wrapper->saved_x = wrapper->scene_tree->node.x;
                wrapper->saved_y = wrapper->scene_tree->node.y;
                wrapper->saved_width = wrapper->toplevel->base->current.geometry.width;
                wrapper->saved_height = wrapper->toplevel->base->current.geometry.height;
                if (wrapper->saved_width <= 0) wrapper->saved_width = 800;
                if (wrapper->saved_height <= 0) wrapper->saved_height = 600;
            }
            struct wlr_box output_box = {0, 0, 1280, 800};
            if (!m_outputs.empty()) {
                struct wlr_output* out = m_outputs.front()->get_wlr_output();
                if (out) {
                    wlr_output_effective_resolution(out, &output_box.width, &output_box.height);
                }
            }
            int32_t target_width = output_box.width;
            int32_t target_height = output_box.height - 48; // Exclude top panel
            log::info("[Window] Maximize window to {}x{}", target_width, target_height);
            wlr_scene_node_set_position(&wrapper->scene_tree->node, 0, 48);
            wlr_xdg_toplevel_set_maximized(wrapper->toplevel, true);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, target_width, target_height);
        } else {
            log::info("[Window] Restore maximized window to {}x{}", wrapper->saved_width, wrapper->saved_height);
            wlr_scene_node_set_position(&wrapper->scene_tree->node, wrapper->saved_x, wrapper->saved_y);
            wlr_xdg_toplevel_set_maximized(wrapper->toplevel, false);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, wrapper->saved_width, wrapper->saved_height);
        }
        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
    }

    void toplevel_set_fullscreen(ToplevelWrapper* wrapper, bool fullscreen) {
        if (wrapper->is_fullscreen == fullscreen) {
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
            }
            struct wlr_box output_box = {0, 0, 1280, 800};
            if (!m_outputs.empty()) {
                struct wlr_output* out = m_outputs.front()->get_wlr_output();
                if (out) {
                    wlr_output_effective_resolution(out, &output_box.width, &output_box.height);
                }
            }
            log::info("[Window] Fullscreen window to {}x{}", output_box.width, output_box.height);
            wlr_scene_node_set_position(&wrapper->scene_tree->node, 0, 0);
            wlr_xdg_toplevel_set_fullscreen(wrapper->toplevel, true);
            wlr_xdg_toplevel_set_size(wrapper->toplevel, output_box.width, output_box.height);
        } else {
            if (wrapper->is_maximized) {
                wrapper->is_maximized = false; // reset flag to trigger correct resize logic
                toplevel_set_maximized(wrapper, true);
            } else {
                log::info("[Window] Restore fullscreen window to {}x{}", wrapper->saved_width, wrapper->saved_height);
                wlr_scene_node_set_position(&wrapper->scene_tree->node, wrapper->saved_x, wrapper->saved_y);
                wlr_xdg_toplevel_set_fullscreen(wrapper->toplevel, false);
                wlr_xdg_toplevel_set_size(wrapper->toplevel, wrapper->saved_width, wrapper->saved_height);
            }
        }
        wlr_xdg_surface_schedule_configure(wrapper->toplevel->base);
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

        self->m_toplevels.push_back(std::move(wrapper));
    }

    static void handle_toplevel_map(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, map);
        struct wlr_surface* surface = wrapper->toplevel->base->surface;
        const char* app_id = wrapper->toplevel->app_id ? wrapper->toplevel->app_id : "";
        log::info("[XDGShell] Toplevel mapped — app_id='{}' surface={}",
                  app_id, static_cast<void*>(surface));

        // If the lock screen just connected, track surface pointer and mark session locked
        if (std::string(app_id) == "lock" || std::string(app_id) == "tinexus-lock") {
            log::info("[XDGShell] Lock screen mapped — m_lock_surface={} session LOCKED", static_cast<void*>(surface));
            wrapper->backend->m_is_locked = true;
            wrapper->backend->m_lock_surface = surface;
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
        log::info("[XDGShell] Request minimize (not implemented) - acknowledging");
        wlr_xdg_surface_schedule_configure(toplevel->base);
    }

    static void handle_toplevel_destroy(struct wl_listener* listener, void* /*data*/) {
        ToplevelWrapper* wrapper = wl_container_of(listener, wrapper, destroy);
        WlrootsBackend* backend = wrapper->backend;
        struct wlr_surface* surface = wrapper->toplevel->base->surface;

        const bool was_lock = (surface == backend->m_lock_surface);
        if (was_lock) {
            log::info("[XDGShell] Lock screen destroyed — marking session UNLOCKED atomically");
            backend->m_is_locked = false;
            backend->m_lock_surface = nullptr;
        }

        if (backend->m_active_toplevel == wrapper) {
            backend->m_active_toplevel = nullptr;
        }

        wl_list_remove(&wrapper->map.link);
        wl_list_remove(&wrapper->commit.link);
        wl_list_remove(&wrapper->destroy.link);
        wl_list_remove(&wrapper->request_maximize.link);
        wl_list_remove(&wrapper->request_fullscreen.link);
        wl_list_remove(&wrapper->request_minimize.link);

        // Remove from list
        for (auto it = backend->m_toplevels.begin(); it != backend->m_toplevels.end(); ++it) {
            if (it->get() == wrapper) {
                backend->m_toplevels.erase(it);
                break;
            }
        }

        // Restore focus to top available toplevel immediately without gap
        if (backend->m_active_toplevel == nullptr) {
            ToplevelWrapper* next_focus = nullptr;
            if (!backend->m_toplevels.empty()) {
                next_focus = backend->m_toplevels.back().get();
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
                wlr_cursor_attach_input_device(self->m_cursor, device);
                break;
            case WLR_INPUT_DEVICE_TABLET:
                log::info("[Input] Detected Tablet (Pointer): {}", device->name);
                wlr_cursor_attach_input_device(self->m_cursor, device);
                break;
            case WLR_INPUT_DEVICE_TOUCH:
                log::info("[Input] Detected Touchscreen: {}", device->name);
                wlr_cursor_attach_input_device(self->m_cursor, device);
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
            if (ShortcutEngine::instance().process_key_event(wlr_mods, event->keycode, is_pressed)) {
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

    static void handle_keyboard_destroy(struct wl_listener* listener, void* data) {
        KeyboardWrapper* wrapper = wl_container_of(listener, wrapper, destroy);
        wl_list_remove(&wrapper->modifiers.link);
        wl_list_remove(&wrapper->key.link);
        wl_list_remove(&wrapper->destroy.link);
        // Remove from m_keyboards logic (simplified for now)
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
        // [3D.2] Cursor image — keep hardware cursor visible
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");

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
                    self->focus_toplevel(clicked_wrapper);
                }
            }
        }

        SeatManager::instance().notify_button(
            event->time_msec, event->button,
            static_cast<uint32_t>(event->state));
    }

    static void handle_cursor_axis(struct wl_listener* listener, void* data) {
        auto* event = static_cast<struct wlr_pointer_axis_event*>(data);
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
