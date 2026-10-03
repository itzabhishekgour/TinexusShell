#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

static int found_screencopy = 0;
static int found_dmabuf = 0;

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    printf("[Global %u] %s (v%u)\n", name, interface, version);
    if (strcmp(interface, "zwlr_screencopy_manager_v1") == 0) {
        found_screencopy = 1;
    } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
        found_dmabuf = 1;
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

int main() {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(display);

    printf("\n--- Protocol Verification ---\n");
    printf("zwlr_screencopy_manager_v1: %s\n", found_screencopy ? "PRESENT (VERIFIED)" : "MISSING");
    printf("zwp_linux_dmabuf_v1:        %s\n", found_dmabuf ? "PRESENT (VERIFIED)" : "MISSING");

    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    if (found_screencopy && found_dmabuf) {
        printf("\n[SUCCESS] Both protocols active and verified on compositor display!\n");
        return 0;
    }
    return 1;
}
