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

void FocusManager::bind(struct wlr_seat* seat, struct wlr_scene* scene) noexcept {
    m_seat  = seat;
    m_scene = scene;
    log::info("[Focus] FocusManager bound — seat={} scene={}",
              static_cast<void*>(seat), static_cast<void*>(scene));
}

// ---------------------------------------------------------------------------
// Pointer focus — scene-graph hit test
// ---------------------------------------------------------------------------

PickResult FocusManager::pick_surface(double x, double y) const noexcept {
    if (!m_scene) {
        return {};
    }

    double sx{0.0}, sy{0.0};
    struct wlr_scene_node* node =
        wlr_scene_node_at(&m_scene->tree.node, x, y, &sx, &sy);

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
        // Entering a new surface or staying on the same one
        if (m_pointer_surface != pick.surface) {
            // Log surface transition for debugging
            log::info("[Focus] Pointer Enter surface={} sx={:.1f} sy={:.1f}",
                      static_cast<void*>(pick.surface), pick.sx, pick.sy);
            m_pointer_surface = pick.surface;
        }

        wlr_seat_pointer_notify_enter(m_seat, pick.surface, pick.sx, pick.sy);
        wlr_seat_pointer_notify_motion(m_seat, time_msec, pick.sx, pick.sy);

        log::info("[Focus] Pointer Motion surface={} sx={:.1f} sy={:.1f}",
                  static_cast<void*>(pick.surface), pick.sx, pick.sy);
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

// ---------------------------------------------------------------------------
// Keyboard focus — real implementation (Phase 4)
// ---------------------------------------------------------------------------

void FocusManager::set_keyboard_focus(struct wlr_surface* surface) noexcept {
    if (!m_seat) {
        return;
    }

    // Avoid redundant re-enters
    if (surface == m_keyboard_surface) {
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
            log::warn("[Focus] set_keyboard_focus: no keyboard attached to seat yet");
        }
    } else {
        wlr_seat_keyboard_notify_clear_focus(m_seat);
        log::info("[Focus] Keyboard cleared");
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
