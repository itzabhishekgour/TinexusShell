#pragma once

#include <wayland-client.h>
#define namespace wl_namespace
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#undef namespace
#include <cstdint>

struct ClientState {
    struct wl_display* display{nullptr};
    struct wl_registry* registry{nullptr};
    struct wl_compositor* compositor{nullptr};
    struct wl_shm* shm{nullptr};
    struct wl_output* output{nullptr};
    struct wl_seat* seat{nullptr};
    struct xdg_wm_base* xdg_wm_base{nullptr};
    struct zwlr_layer_shell_v1* layer_shell{nullptr};

    bool init();
    void cleanup();
};
