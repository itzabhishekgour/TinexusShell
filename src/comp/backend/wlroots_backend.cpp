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
#include "comp/cursor/cursor_manager.hpp"

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
        m_wlr_compositor = wlr_compositor_create(m_display, 5, m_wlr_renderer);
        if (!m_wlr_compositor) {
            log::error("[Backend] Failed to create wlroots compositor.");
            return false;
        }

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
        m_scene_tree_bottom = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_normal = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_top = wlr_scene_tree_create(&m_scene->tree);
        m_scene_tree_overlay = wlr_scene_tree_create(&m_scene->tree);

        m_seat = wlr_seat_create(m_display, "seat0");
        SeatManager::instance().bind_seat("seat0");

        m_cursor = wlr_cursor_create();
        wlr_cursor_attach_output_layout(m_cursor, m_output_layout);
        m_cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);
        wlr_xcursor_manager_load(m_cursor_mgr, 1.0f);
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");

        m_layer_shell = wlr_layer_shell_v1_create(m_display, 4);
        m_new_layer_surface_listener.notify = handle_new_layer_surface;
        wl_signal_add(&m_layer_shell->events.new_surface, &m_new_layer_surface_listener);

        m_xdg_shell = wlr_xdg_shell_create(m_display, 3);
        m_new_xdg_surface_listener.notify = handle_new_xdg_surface;
        wl_signal_add(&m_xdg_shell->events.new_surface, &m_new_xdg_surface_listener);

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

private:
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

    struct KeyboardWrapper {
        struct wl_listener modifiers;
        struct wl_listener key;
        struct wl_listener destroy;
        struct wlr_keyboard* keyboard;
        WlrootsBackend* backend;
    };
    std::vector<std::unique_ptr<KeyboardWrapper>> m_keyboards;

    struct LayerSurfaceWrapper {
        struct wlr_layer_surface_v1* layer_surface{nullptr};
        struct wlr_scene_layer_surface_v1* scene_layer{nullptr};
        struct wl_listener destroy;
        struct wl_listener commit;
    };

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
            if (w->layer_surface->initialized) {
                struct wlr_box full_area = {0, 0, 0, 0};
                if (w->layer_surface->output) {
                    wlr_output_effective_resolution(w->layer_surface->output, &full_area.width, &full_area.height);
                }
                struct wlr_box usable_area = full_area;
                wlr_scene_layer_surface_v1_configure(w->scene_layer, &full_area, &usable_area);
            }
        };
        wl_signal_add(&layer_surface->surface->events.commit, &wrapper->commit);
    }

    static void handle_new_xdg_surface(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_xdg_surface_listener);
        auto* xdg_surface = static_cast<struct wlr_xdg_surface*>(data);

        if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
            log::info("[XDGShell] New XDG toplevel surface created");
            wlr_scene_xdg_surface_create(self->m_scene_tree_normal, xdg_surface);
        }
    }

    static void handle_new_output(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_output_listener);
        auto* wlr_out = static_cast<struct wlr_output*>(data);

        auto output = std::make_unique<TinexusOutput>(wlr_out, self->m_wlr_allocator, self->m_wlr_renderer, self->m_scene);
        if (output->initialize()) {
            wlr_output_layout_add_auto(self->m_output_layout, wlr_out);

            // CRITICAL: Register this output with the scene graph.
            // Without this, wlr_scene_get_scene_output() returns NULL in frame()
            // and nothing ever gets rendered to screen.
            struct wlr_scene_output* scene_out = wlr_scene_output_create(self->m_scene, wlr_out);
            if (!scene_out) {
                log::error("[Backend] Failed to create scene output for '{}'", wlr_out->name);
            } else {
                log::info("[Backend] Scene output registered for '{}'", wlr_out->name);
            }

            self->m_outputs.push_back(std::move(output));
        } else {
            log::error("[Backend] Failed to initialize TinexusOutput for '{}'", wlr_out->name);
        }
    }

    static void handle_new_input(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_new_input_listener);
        auto* device = static_cast<struct wlr_input_device*>(data);

        switch (device->type) {
            case WLR_INPUT_DEVICE_KEYBOARD:
                log::info("[Input] Detected Keyboard: {}", device->name);
                self->setup_keyboard(device);
                break;
            case WLR_INPUT_DEVICE_POINTER:
                log::info("[Input] Detected Pointer: {}", device->name);
                wlr_cursor_attach_input_device(self->m_cursor, device);
                break;
            default:
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
        wlr_seat_set_keyboard(wrapper->backend->m_seat, wrapper->keyboard);
        wlr_seat_keyboard_notify_modifiers(wrapper->backend->m_seat, &wrapper->keyboard->modifiers);
    }

    static void handle_keyboard_key(struct wl_listener* listener, void* data) {
        KeyboardWrapper* wrapper = wl_container_of(listener, wrapper, key);
        auto* event = static_cast<struct wlr_keyboard_key_event*>(data);
        
        uint32_t keycode = event->keycode + 8;
        const xkb_keysym_t* syms;
        int nsyms = xkb_state_key_get_syms(wrapper->keyboard->xkb_state, keycode, &syms);

        if (nsyms > 0) {
            log::info("[Keyboard] Key {} state {}", syms[0], static_cast<uint32_t>(event->state));
        }

        wlr_seat_set_keyboard(wrapper->backend->m_seat, wrapper->keyboard);
        wlr_seat_keyboard_notify_key(wrapper->backend->m_seat, event->time_msec, event->keycode, event->state);
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
        // Set cursor image so it renders on screen
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
        
        int32_t cx = static_cast<int32_t>(std::round(m_cursor->x));
        int32_t cy = static_cast<int32_t>(std::round(m_cursor->y));
        SeatManager::instance().send_pointer_motion(cx, cy);
        CursorManager::instance().update_position(cx, cy);
    }

    static void handle_cursor_button(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_button_listener);
        auto* event = static_cast<struct wlr_pointer_button_event*>(data);
        SeatManager::instance().send_button_click(event->button, event->state);
        wlr_seat_pointer_notify_button(self->m_seat, event->time_msec, event->button, event->state);
    }

    static void handle_cursor_axis(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_axis_listener);
        auto* event = static_cast<struct wlr_pointer_axis_event*>(data);
        wlr_seat_pointer_notify_axis(self->m_seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
    }

    static void handle_cursor_frame(struct wl_listener* listener, void* data) {
        WlrootsBackend* self = wl_container_of(listener, self, m_cursor_frame_listener);
        wlr_seat_pointer_notify_frame(self->m_seat);
    }
};

std::unique_ptr<Backend> create_wlroots_backend(struct wl_display* display) {
    return std::make_unique<WlrootsBackend>(display);
}

} // namespace tinexus::comp
