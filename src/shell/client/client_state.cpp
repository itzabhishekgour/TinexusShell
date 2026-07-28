#include "client_state.hpp"
#include "common/logger.hpp"
#include <cstring>
#include <iostream>

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto* state = static_cast<ClientState*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = static_cast<struct wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        state->output = static_cast<struct wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = static_cast<struct wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = static_cast<struct zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
    }
}

static void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    // Left blank for now
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

bool ClientState::init() {
    display = wl_display_connect(nullptr);
    if (!display) {
        tinexus::log::error("Failed to connect to Wayland display");
        return false;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);

    wl_display_roundtrip(display);

    if (!compositor || !shm || !layer_shell || !output) {
        tinexus::log::error("Missing required Wayland globals");
        return false;
    }

    tinexus::log::info("Successfully bound to Wayland globals.");
    return true;
}

void ClientState::cleanup() {
    if (layer_shell) zwlr_layer_shell_v1_destroy(layer_shell);
    if (xdg_wm_base) xdg_wm_base_destroy(xdg_wm_base);
    if (seat) wl_seat_destroy(seat);
    if (output) wl_output_destroy(output);
    if (shm) wl_shm_destroy(shm);
    if (compositor) wl_compositor_destroy(compositor);
    if (registry) wl_registry_destroy(registry);
    if (display) wl_display_disconnect(display);
}
