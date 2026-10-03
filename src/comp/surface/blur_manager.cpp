// ============================================================================
// blur_manager.cpp — Wayland Blur Protocol Manager for Tinexus Compositor
// ============================================================================
#include "comp/surface/blur_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
#include "org-kde-kwin-blur-protocol.h"
}

namespace tinexus::comp {

BlurConfig BlurConfig::load_from_config(const std::string& path) {
    BlurConfig cfg;
    std::string config_file = path;
    if (config_file.empty()) {
        const char* home = getenv("HOME");
        if (home && *home) {
            std::string user_cfg = std::string(home) + "/.config/tinexus/compositor.toml";
            if (std::filesystem::exists(user_cfg)) {
                config_file = user_cfg;
            }
        }
        if (config_file.empty()) {
            std::string sys_cfg = "/etc/tinexus/defaults/compositor.toml";
            if (std::filesystem::exists(sys_cfg)) {
                config_file = sys_cfg;
            }
        }
    }

    if (config_file.empty() || !std::filesystem::exists(config_file)) {
        return cfg;
    }

    std::ifstream f(config_file);
    if (!f.is_open()) return cfg;

    std::string line;
    bool in_blur_section = false;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) line.pop_back();

        if (line.starts_with("[")) {
            in_blur_section = (line == "[blur]" || line == "[appearance]");
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '"' || val.back() == '\'')) val.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t' || val.front() == '"' || val.front() == '\'')) val.erase(val.begin());

        if (in_blur_section) {
            if (key == "radius" || key == "backdrop_blur_radius") {
                try { cfg.radius = std::stoi(val); } catch (...) {}
            } else if (key == "tint") {
                try {
                    if (val.starts_with("#")) val.erase(val.begin());
                    cfg.tint = static_cast<uint32_t>(std::stoul(val, nullptr, 16));
                } catch (...) {}
            }
        }
    }
    return cfg;
}

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

    load_config();

    m_global = wl_global_create(display, &org_kde_kwin_blur_manager_interface, 1, this, blur_manager_bind);
    if (!m_global) {
        log::error("[BlurManager] Failed to create org_kde_kwin_blur_manager global");
        return false;
    }

    log::info("[BlurManager] Successfully registered org_kde_kwin_blur_manager Wayland protocol global (radius={}, tint=0x{:08X})",
              m_config.radius, m_config.tint);
    return true;
}

void BlurManager::set_config(const BlurConfig& config) {
    m_config = config;
    for (auto& [surf, state] : m_surfaces) {
        if (state) {
            state->radius = m_config.radius;
            state->tint = m_config.tint;
        }
    }
}

void BlurManager::load_config(const std::string& path) {
    set_config(BlurConfig::load_from_config(path));
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
        state->radius = m_config.radius;
        state->tint = m_config.tint;

        state->destroy_listener.notify = [](struct wl_listener* l, void* /*data*/) {
            BlurSurfaceState* s = wl_container_of(l, s, destroy_listener);
            if (s && s->surface) {
                BlurManager::instance().unregister_surface_blur(s->surface);
            }
        };
        wl_signal_add(&surface->events.destroy, &state->destroy_listener);

        if (blur_resource) {
            wl_resource_set_implementation(blur_resource, &blur_implementation, state.get(), blur_resource_destroy);
        }
        m_surfaces[surface] = std::move(state);
    } else {
        if (blur_resource) {
            wl_resource_set_implementation(blur_resource, &blur_implementation, it->second.get(), blur_resource_destroy);
        }
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
        if (it->second->destroy_listener.link.next) {
            wl_list_remove(&it->second->destroy_listener.link);
            it->second->destroy_listener.link.next = nullptr;
            it->second->destroy_listener.link.prev = nullptr;
        }
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
