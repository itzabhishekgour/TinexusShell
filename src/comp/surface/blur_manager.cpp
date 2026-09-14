// ============================================================================
// blur_manager.cpp — Wayland Blur Protocol Manager for Tinexus Compositor
// ============================================================================
#include "comp/surface/blur_manager.hpp"
#include "common/logger.hpp"
extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
#include "org-kde-kwin-blur-protocol.h"
}

namespace tinexus::comp {

namespace {

void blur_handle_commit(struct wl_client* /*client*/, struct wl_resource* resource) {
    auto* state = static_cast<BlurSurfaceState*>(wl_resource_get_user_data(resource));
    if (state && state->surface) {
        BlurManager::instance().commit_surface_blur(state->surface);
    }
}

void blur_handle_set_region(struct wl_client* /*client*/, struct wl_resource* resource, struct wl_resource* region_resource) {
    auto* state = static_cast<BlurSurfaceState*>(wl_resource_get_user_data(resource));
    if (!state || !state->surface) return;

    if (region_resource) {
        const pixman_region32_t* reg = wlr_region_from_resource(region_resource);
        if (reg) {
            pixman_region32_copy(&state->region, reg);
            state->has_custom_region = true;
        }
    } else {
        pixman_region32_clear(&state->region);
        state->has_custom_region = false;
    }
}

void blur_handle_release(struct wl_client* /*client*/, struct wl_resource* resource) {
    wl_resource_destroy(resource);
}

const struct org_kde_kwin_blur_interface blur_implementation = {
    .commit = blur_handle_commit,
    .set_region = blur_handle_set_region,
    .release = blur_handle_release,
};

void blur_resource_destroy(struct wl_resource* resource) {
    // Resources are cleaned up on client disconnect
    (void)resource;
}

void blur_manager_handle_create(struct wl_client* client, struct wl_resource* /*resource*/,
                               uint32_t id, struct wl_resource* surface_resource) {
    struct wlr_surface* surface = wlr_surface_from_resource(surface_resource);
    if (!surface) return;

    struct wl_resource* blur_res = wl_resource_create(client, &org_kde_kwin_blur_interface, 1, id);
    if (!blur_res) {
        wl_client_post_no_memory(client);
        return;
    }

    BlurManager::instance().register_surface_blur(surface, blur_res);
}

void blur_manager_handle_unset(struct wl_client* /*client*/, struct wl_resource* /*resource*/,
                              struct wl_resource* surface_resource) {
    struct wlr_surface* surface = wlr_surface_from_resource(surface_resource);
    if (!surface) return;

    BlurManager::instance().unregister_surface_blur(surface);
}

const struct org_kde_kwin_blur_manager_interface blur_manager_implementation = {
    .create = blur_manager_handle_create,
    .unset = blur_manager_handle_unset,
};

void blur_manager_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id) {
    (void)data;
    struct wl_resource* resource = wl_resource_create(client, &org_kde_kwin_blur_manager_interface,
                                                      std::min(version, 1u), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &blur_manager_implementation, nullptr, nullptr);
}

} // anonymous namespace

BlurManager& BlurManager::instance() {
    static BlurManager s_instance;
    return s_instance;
}

BlurManager::~BlurManager() {
    shutdown();
}

bool BlurManager::initialize(struct wl_display* display) {
    if (!display) return false;
    m_display = display;

    m_global = wl_global_create(display, &org_kde_kwin_blur_manager_interface, 1, this, blur_manager_bind);
    if (!m_global) {
        log::error("[BlurManager] Failed to create org_kde_kwin_blur_manager global");
        return false;
    }

    log::info("[BlurManager] Successfully registered org_kde_kwin_blur_manager Wayland protocol global");
    return true;
}

void BlurManager::shutdown() {
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
    m_surfaces.clear();
}

void BlurManager::register_surface_blur(struct wlr_surface* surface, struct wl_resource* blur_resource) {
    if (!surface) return;

    auto it = m_surfaces.find(surface);
    if (it == m_surfaces.end()) {
        auto state = std::make_unique<BlurSurfaceState>();
        state->surface = surface;
        pixman_region32_init(&state->region);
        state->has_custom_region = false;
        state->radius = 28;
        state->tint = 0x13131ACC;

        state->destroy_listener.notify = [](struct wl_listener* l, void* /*data*/) {
            BlurSurfaceState* s = wl_container_of(l, s, destroy_listener);
            if (s && s->surface) {
                BlurManager::instance().unregister_surface_blur(s->surface);
            }
        };
        wl_signal_add(&surface->events.destroy, &state->destroy_listener);

        wl_resource_set_implementation(blur_resource, &blur_implementation, state.get(), blur_resource_destroy);
        m_surfaces[surface] = std::move(state);
    } else {
        wl_resource_set_implementation(blur_resource, &blur_implementation, it->second.get(), blur_resource_destroy);
    }

    log::info("[BlurManager] Registered blur request for surface {:p}", static_cast<void*>(surface));
}

void BlurManager::commit_surface_blur(struct wlr_surface* surface) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        log::info("[BlurManager] Committed blur for surface {:p} (custom_region={})",
                  static_cast<void*>(surface), it->second->has_custom_region);
    }
}

void BlurManager::unregister_surface_blur(struct wlr_surface* surface) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        wl_list_remove(&it->second->destroy_listener.link);
        pixman_region32_fini(&it->second->region);
        m_surfaces.erase(it);
        log::info("[BlurManager] Unregistered blur for surface {:p}", static_cast<void*>(surface));
    }
}

bool BlurManager::is_surface_blurred(struct wlr_surface* surface) const {
    if (!surface) return false;
    return m_surfaces.find(surface) != m_surfaces.end();
}

const BlurSurfaceState* BlurManager::get_surface_state(struct wlr_surface* surface) const {
    if (!surface) return nullptr;
    auto it = m_surfaces.find(surface);
    return (it != m_surfaces.end()) ? it->second.get() : nullptr;
}

bool BlurManager::is_namespace_blurred(const std::string& ns) const {
    return m_blurred_namespaces.find(ns) != m_blurred_namespaces.end();
}

} // namespace tinexus::comp
