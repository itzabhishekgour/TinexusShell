#include "comp/focus/focus_manager.hpp"
#include "common/logger.hpp"

// C++ standard library MUST be included before #define static below.
// GCC 15's <bits/specfun.h> contains `static` member functions that break
// if `static` is redefined to empty. Pre-including these forces instantiation
// while `static` still has its normal meaning.
#include <cmath>
#include <string>
#include <cstdint>

extern "C" {
#define static                     // wlr headers use static keyword — mask it
#include <wlr/types/wlr_scene.h>
#undef static
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>   // wlr_seat_get_keyboard, wlr_keyboard
#include <wlr/types/wlr_compositor.h> // wlr_surface
}


namespace tinexus::comp {

FocusManager& FocusManager::instance() noexcept {
    static FocusManager s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void FocusManager::bind(struct wlr_seat* seat, struct wlr_scene* scene,
                        struct wlr_scene* overlay_scene) noexcept {
    m_seat          = seat;
    m_scene         = scene;
    m_overlay_scene = overlay_scene;
    log::info("[Focus] FocusManager bound — seat={} scene={} overlay_scene={}",
              static_cast<void*>(seat), static_cast<void*>(scene), static_cast<void*>(overlay_scene));
}

// ---------------------------------------------------------------------------
// Pointer focus — scene-graph hit test
// ---------------------------------------------------------------------------

PickResult FocusManager::pick_surface(double x, double y) const noexcept {
    double sx{0.0}, sy{0.0};
    struct wlr_scene_node* node{nullptr};

    if (m_overlay_scene) {
        node = wlr_scene_node_at(&m_overlay_scene->tree.node, x, y, &sx, &sy);
    }
    if (!node && m_scene) {
        node = wlr_scene_node_at(&m_scene->tree.node, x, y, &sx, &sy);
    }

    if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
        return {};
    }

    struct wlr_scene_buffer*  scene_buf =
        wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface* scene_surf =
        wlr_scene_surface_try_from_buffer(scene_buf);

    if (!scene_surf || !scene_surf->surface) {
        return {};
    }

    if (y <= 60.0) {
        log::debug("[Focus] pick_surface ({:.1f}, {:.1f}) -> surf={} sx={:.1f} sy={:.1f}",
                   x, y, static_cast<void*>(scene_surf->surface), sx, sy);
    }

    return PickResult{ scene_surf->surface, sx, sy };
}

// ---------------------------------------------------------------------------
// Pointer focus — seat notification
// ---------------------------------------------------------------------------

void FocusManager::update_pointer_focus(const PickResult& pick,
                                         uint32_t          time_msec) noexcept {
    if (!m_seat) {
        return;
    }

    if (pick.surface) {
        m_last_sx = pick.sx;
        m_last_sy = pick.sy;
        // Entering a new surface or staying on the same one
        if (m_pointer_surface != pick.surface) {
            // Log surface transition for debugging
            log::info("[Focus] Pointer Enter surface={} sx={:.1f} sy={:.1f}",
                      static_cast<void*>(pick.surface), pick.sx, pick.sy);
            m_pointer_surface = pick.surface;
        }

        wlr_seat_pointer_notify_enter(m_seat, pick.surface, pick.sx, pick.sy);
        wlr_seat_pointer_notify_motion(m_seat, time_msec, pick.sx, pick.sy);
    } else {
        // Cursor over empty desktop — clear focus
        if (m_pointer_surface) {
            log::info("[Focus] Pointer Leave surface={}",
                      static_cast<void*>(m_pointer_surface));
            m_pointer_surface = nullptr;
        }
        wlr_seat_pointer_clear_focus(m_seat);
    }
}

void FocusManager::handle_pointer_button(double cursor_x, double cursor_y, bool pressed) noexcept {
    if (pressed) {
        if (m_pressed_buttons == 0 && m_pointer_surface != nullptr) {
            m_pointer_grab_surface = m_pointer_surface;
            m_grab_origin_x = cursor_x - m_last_sx;
            m_grab_origin_y = cursor_y - m_last_sy;

            if (m_grab_destroy_listener.link.next) {
                wl_list_remove(&m_grab_destroy_listener.link);
                m_grab_destroy_listener.link.next = nullptr;
            }
            m_grab_destroy_listener.notify = [](struct wl_listener* l, void* /*data*/) {
                FocusManager* self = wl_container_of(l, self, m_grab_destroy_listener);
                self->m_pointer_grab_surface = nullptr;
                self->m_pressed_buttons = 0;
                if (self->m_grab_destroy_listener.link.next) {
                    wl_list_remove(&self->m_grab_destroy_listener.link);
                    self->m_grab_destroy_listener.link.next = nullptr;
                }
            };
            wl_signal_add(&m_pointer_grab_surface->events.destroy, &m_grab_destroy_listener);
        }
        ++m_pressed_buttons;
    } else {
        if (m_pressed_buttons > 0) {
            --m_pressed_buttons;
        }
        if (m_pressed_buttons == 0) {
            m_pointer_grab_surface = nullptr;
            if (m_grab_destroy_listener.link.next) {
                wl_list_remove(&m_grab_destroy_listener.link);
                m_grab_destroy_listener.link.next = nullptr;
            }
        }
    }
}

bool FocusManager::update_grabbed_pointer_motion(double cursor_x, double cursor_y,
                                                 const PickResult& pick, uint32_t time_msec) noexcept {
    if (!m_seat || !has_pointer_grab()) {
        return false;
    }
    double sx = cursor_x - m_grab_origin_x;
    double sy = cursor_y - m_grab_origin_y;
    if (pick.surface == m_pointer_grab_surface) {
        sx = pick.sx;
        sy = pick.sy;
        m_grab_origin_x = cursor_x - pick.sx;
        m_grab_origin_y = cursor_y - pick.sy;
    }
    m_last_sx = sx;
    m_last_sy = sy;
    wlr_seat_pointer_notify_motion(m_seat, time_msec, sx, sy);
    return true;
}

// ---------------------------------------------------------------------------
// Keyboard focus — real implementation (Phase 4)
// ---------------------------------------------------------------------------

void FocusManager::set_keyboard_focus(struct wlr_surface* surface) noexcept {
    if (!m_seat) {
        return;
    }

    // Avoid redundant re-enters only if the seat actually already has this surface focused
    if (surface == m_keyboard_surface && m_seat->keyboard_state.focused_surface == surface) {
        return;
    }

    m_keyboard_surface = surface;

    if (surface) {
        struct wlr_keyboard* kb = wlr_seat_get_keyboard(m_seat);
        if (kb) {
            wlr_seat_keyboard_notify_enter(
                m_seat, surface,
                kb->keycodes, kb->num_keycodes,
                &kb->modifiers);
            log::info("[Focus] Keyboard → surface={}", static_cast<void*>(surface));
        } else {
            log::warn("[Focus] set_keyboard_focus: no keyboard attached to seat yet (deferred)");
        }
    } else {
        wlr_seat_keyboard_notify_clear_focus(m_seat);
        log::info("[Focus] Keyboard cleared");
    }
}

void FocusManager::reassert_keyboard_focus() noexcept {
    if (!m_seat || !m_keyboard_surface) {
        return;
    }
    struct wlr_keyboard* kb = wlr_seat_get_keyboard(m_seat);
    if (!kb) {
        return;
    }
    if (m_seat->keyboard_state.focused_surface != m_keyboard_surface) {
        wlr_seat_keyboard_notify_enter(
            m_seat, m_keyboard_surface,
            kb->keycodes, kb->num_keycodes,
            &kb->modifiers);
        log::info("[Focus] Reasserted keyboard focus on device attach → surface={}",
                  static_cast<void*>(m_keyboard_surface));
    }
}

// ---------------------------------------------------------------------------
// Legacy API
// ---------------------------------------------------------------------------

void FocusManager::set_focus(FocusTargetType    type,
                              uint64_t           surface_id,
                              const std::string& target_id) {
    m_focus_type = type;
    m_surface_id = surface_id;
    m_target_id  = target_id;

    log::info("[Focus] Legacy set_focus TargetType={} SurfaceID={} TargetID='{}'",
              focus_target_to_string(type), surface_id, target_id);
}

} // namespace tinexus::comp
