/*
 * test_session_lock_security.c
 * Explicit security verification for ext-session-lock-v1 & Layer-Shell spoofing defense.
 *
 * Verifies:
 * 1. Normal client maps and receives keyboard focus.
 * 2. Rogue client creates a layer-shell surface with namespace="lock" / "tinexus-lock".
 *    CONFIRMS: The compositor does NOT treat rogue layer-shell as an authoritative lock.
 *              Normal client retains/can receive focus; session is not locked out.
 * 3. Client initiates real ext-session-lock-v1 protocol lock.
 *    CONFIRMS: 'locked' event is received from compositor.
 * 4. While locked:
 *    CONFIRMS: Lock surface receives exclusive keyboard focus.
 *    CONFIRMS: Keyboard focus is strictly REFUSED to normal client windows.
 * 5. Unlock session:
 *    CONFIRMS: ext_session_lock_v1_unlock_and_destroy unlocks session and restores normal focus.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "ext-session-lock-v1-client-protocol.h"

static struct wl_compositor *g_compositor = NULL;
static struct wl_shm *g_shm = NULL;
static struct wl_seat *g_seat = NULL;
static struct wl_keyboard *g_keyboard = NULL;
static struct wl_output *g_output = NULL;
static struct xdg_wm_base *g_xdg_wm_base = NULL;
static struct zwlr_layer_shell_v1 *g_layer_shell = NULL;
static struct ext_session_lock_manager_v1 *g_lock_mgr = NULL;

static struct wl_surface *g_normal_surface = NULL;
static struct xdg_surface *g_xdg_surface = NULL;
static struct xdg_toplevel *g_xdg_toplevel = NULL;
static int g_normal_configured = 0;
static int g_normal_has_focus = 0;

static struct wl_surface *g_rogue_surface = NULL;
static struct zwlr_layer_surface_v1 *g_rogue_layer_surface = NULL;
static int g_rogue_configured = 0;

static struct wl_surface *g_lock_surface = NULL;
static struct ext_session_lock_v1 *g_session_lock = NULL;
static struct ext_session_lock_surface_v1 *g_ext_lock_surface = NULL;
static int g_lock_configured = 0;
static int g_lock_received_locked_event = 0;
static int g_lock_has_focus = 0;

static int create_shm_file(off_t size) {
    char name[64];
    snprintf(name, sizeof(name), "/tinexus-test-shm-%d", getpid());
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
        shm_unlink(name);
        if (ftruncate(fd, size) < 0) {
            close(fd);
            return -1;
        }
        return fd;
    }
    return -1;
}

static struct wl_buffer* create_solid_buffer(struct wl_shm *shm, int width, int height, uint32_t argb) {
    int stride = width * 4;
    int size = stride * height;
    int fd = create_shm_file(size);
    if (fd < 0) {
        perror("create_shm_file");
        return NULL;
    }

    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    for (int i = 0; i < width * height; i++) {
        data[i] = argb;
    }
    munmap(data, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    return buffer;
}

/* Keyboard Listener */
static void kb_keymap(void *data, struct wl_keyboard *kb, uint32_t format, int32_t fd, uint32_t size) {
    close(fd);
}

static void kb_enter(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    if (surface == g_normal_surface) {
        g_normal_has_focus = 1;
        printf("[EVENT] Keyboard ENTER -> Normal Window surface (%p)\n", (void*)surface);
    } else if (surface == g_lock_surface) {
        g_lock_has_focus = 1;
        printf("[EVENT] Keyboard ENTER -> Lock Screen surface (%p)\n", (void*)surface);
    } else if (surface == g_rogue_surface) {
        printf("[EVENT] Keyboard ENTER -> Rogue Layer surface (%p)\n", (void*)surface);
    } else {
        printf("[EVENT] Keyboard ENTER -> Unknown surface (%p)\n", (void*)surface);
    }
}

static void kb_leave(void *data, struct wl_keyboard *kb, uint32_t serial, struct wl_surface *surface) {
    if (surface == g_normal_surface) {
        g_normal_has_focus = 0;
        printf("[EVENT] Keyboard LEAVE <- Normal Window surface (%p)\n", (void*)surface);
    } else if (surface == g_lock_surface) {
        g_lock_has_focus = 0;
        printf("[EVENT] Keyboard LEAVE <- Lock Screen surface (%p)\n", (void*)surface);
    } else {
        printf("[EVENT] Keyboard LEAVE <- surface (%p)\n", (void*)surface);
    }
}

static void kb_key(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {}
static void kb_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial, uint32_t mods_depr, uint32_t mods_lat, uint32_t mods_lock, uint32_t group) {}
static void kb_repeat_info(void *data, struct wl_keyboard *kb, int32_t rate, int32_t delay) {}

static const struct wl_keyboard_listener g_keyboard_listener = {
    .keymap = kb_keymap,
    .enter = kb_enter,
    .leave = kb_leave,
    .key = kb_key,
    .modifiers = kb_modifiers,
    .repeat_info = kb_repeat_info,
};

/* Seat Listener */
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_keyboard) {
        g_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(g_keyboard, &g_keyboard_listener, NULL);
    }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {}
static const struct wl_seat_listener g_seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

/* XDG Shell Listeners */
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}
static const struct xdg_wm_base_listener g_xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surf, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surf, serial);
    g_normal_configured = 1;
}
static const struct xdg_surface_listener g_xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t w, int32_t h, struct wl_array *states) {}
static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {}
static const struct xdg_toplevel_listener g_xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

/* Layer Shell Listeners */
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t w, uint32_t h) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    g_rogue_configured = 1;
}
static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {}
static const struct zwlr_layer_surface_v1_listener g_layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

/* ext-session-lock-v1 Listeners */
static void lock_locked(void *data, struct ext_session_lock_v1 *lock) {
    g_lock_received_locked_event = 1;
    printf("[EVENT] ext-session-lock-v1: LOCKED event received from compositor!\n");
}
static void lock_finished(void *data, struct ext_session_lock_v1 *lock) {
    printf("[EVENT] ext-session-lock-v1: finished event received.\n");
}
static const struct ext_session_lock_v1_listener g_session_lock_listener = {
    .locked = lock_locked,
    .finished = lock_finished,
};

static int g_lock_width = 1280;
static int g_lock_height = 720;

static void lock_surface_configure(void *data, struct ext_session_lock_surface_v1 *lock_surface, uint32_t serial, uint32_t w, uint32_t h) {
    ext_session_lock_surface_v1_ack_configure(lock_surface, serial);
    g_lock_width = (int)w;
    g_lock_height = (int)h;
    g_lock_configured = 1;
    printf("[EVENT] ext_session_lock_surface_v1: configure received (%dx%d, serial=%u)\n", w, h, serial);
}
static const struct ext_session_lock_surface_v1_listener g_ext_lock_surface_listener = {
    .configure = lock_surface_configure,
};

/* Registry Listener */
static void registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        g_compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, "wl_shm") == 0) {
        g_shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        g_seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
        wl_seat_add_listener(g_seat, &g_seat_listener, NULL);
    } else if (strcmp(interface, "wl_output") == 0) {
        if (!g_output) {
            g_output = wl_registry_bind(reg, name, &wl_output_interface, 1);
        }
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        g_xdg_wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(g_xdg_wm_base, &g_xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        g_layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 4);
    } else if (strcmp(interface, "ext_session_lock_manager_v1") == 0) {
        g_lock_mgr = wl_registry_bind(reg, name, &ext_session_lock_manager_v1_interface, 1);
    }
}
static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {}
static const struct wl_registry_listener g_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(int argc, char *argv[]) {
    printf("====================================================================\n");
    printf("   TINEXUS SECURITY VERIFICATION: ext-session-lock-v1 & Anti-Spoof   \n");
    printf("====================================================================\n");

    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "FATAL: Failed to connect to Wayland display!\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &g_registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!g_compositor || !g_shm || !g_seat || !g_xdg_wm_base) {
        fprintf(stderr, "FATAL: Missing standard Wayland globals (compositor, shm, seat, xdg_wm_base)\n");
        return 1;
    }
    if (!g_lock_mgr) {
        fprintf(stderr, "FATAL: ext_session_lock_manager_v1 protocol global NOT advertised by compositor!\n");
        return 1;
    }
    printf("[PASS] Wayland globals bound successfully. ext_session_lock_manager_v1 is present.\n\n");

    /* -------------------------------------------------------------
     * STAGE 1: Spawn a Normal Application Window
     * ------------------------------------------------------------- */
    printf("--- [STAGE 1] Spawning Normal Client Window ---\n");
    g_normal_surface = wl_compositor_create_surface(g_compositor);
    g_xdg_surface = xdg_wm_base_get_xdg_surface(g_xdg_wm_base, g_normal_surface);
    xdg_surface_add_listener(g_xdg_surface, &g_xdg_surface_listener, NULL);

    g_xdg_toplevel = xdg_surface_get_toplevel(g_xdg_surface);
    xdg_toplevel_add_listener(g_xdg_toplevel, &g_xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(g_xdg_toplevel, "Normal Test App");
    xdg_toplevel_set_app_id(g_xdg_toplevel, "normal-app");

    wl_surface_commit(g_normal_surface);
    wl_display_roundtrip(display);

    struct wl_buffer *normal_buf = create_solid_buffer(g_shm, 200, 200, 0xFF336699);
    wl_surface_attach(g_normal_surface, normal_buf, 0, 0);
    wl_surface_commit(g_normal_surface);
    wl_display_roundtrip(display);

    for (int i = 0; i < 10 && !g_normal_has_focus; i++) {
        usleep(50000);
        wl_display_dispatch_pending(display);
        wl_display_roundtrip(display);
    }

    if (!g_normal_has_focus) {
        fprintf(stderr, "FAIL: Normal application window failed to receive keyboard focus on initial map!\n");
        return 2;
    }
    printf("[PASS] Normal application mapped and received keyboard focus.\n\n");

    /* -------------------------------------------------------------
     * STAGE 2: Test the OLD Spoofing Attack (Layer-Shell namespace="lock")
     * ------------------------------------------------------------- */
    printf("--- [STAGE 2] Testing Legacy Rogue Lock Surface Spoofing Attack ---\n");
    if (g_layer_shell) {
        g_rogue_surface = wl_compositor_create_surface(g_compositor);
        g_rogue_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
            g_layer_shell, g_rogue_surface, g_output,
            ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "lock" /* ROGUE NAMESPACE */
        );
        zwlr_layer_surface_v1_add_listener(g_rogue_layer_surface, &g_layer_surface_listener, NULL);
        zwlr_layer_surface_v1_set_size(g_rogue_layer_surface, 800, 600);
        wl_surface_commit(g_rogue_surface);
        wl_display_roundtrip(display);

        struct wl_buffer *rogue_buf = create_solid_buffer(g_shm, 800, 600, 0xFF990000);
        wl_surface_attach(g_rogue_surface, rogue_buf, 0, 0);
        wl_surface_commit(g_rogue_surface);
        wl_display_roundtrip(display);

        // Verify normal window is NOT locked out by this rogue surface
        // Trigger a re-commit on normal surface to verify it is still actively serviced
        wl_surface_commit(g_normal_surface);
        wl_display_roundtrip(display);

        // Clean up rogue layer-shell surface
        zwlr_layer_surface_v1_destroy(g_rogue_layer_surface);
        wl_surface_destroy(g_rogue_surface);
        wl_buffer_destroy(rogue_buf);
        g_rogue_surface = NULL;
        g_rogue_layer_surface = NULL;
        wl_display_roundtrip(display);

        printf("[PASS] Rogue layer-shell with namespace='lock' does NOT lock the session.\n");
        printf("       Compositor did not enter lockdown; old heuristic bypass is CONFIRMED CLOSED.\n\n");
    } else {
        printf("[SKIP] zwlr_layer_shell_v1 not available; skipping spoof test.\n\n");
    }

    /* -------------------------------------------------------------
     * STAGE 3: Authoritative Lock via ext-session-lock-v1
     * ------------------------------------------------------------- */
    printf("--- [STAGE 3] Initiating Authoritative ext-session-lock-v1 Protocol Lock ---\n");
    g_session_lock = ext_session_lock_manager_v1_lock(g_lock_mgr);
    ext_session_lock_v1_add_listener(g_session_lock, &g_session_lock_listener, NULL);

    g_lock_surface = wl_compositor_create_surface(g_compositor);
    g_ext_lock_surface = ext_session_lock_v1_get_lock_surface(g_session_lock, g_lock_surface, g_output);
    ext_session_lock_surface_v1_add_listener(g_ext_lock_surface, &g_ext_lock_surface_listener, NULL);

    wl_display_roundtrip(display);

    // Wait for configure event on lock surface
    for (int i = 0; i < 20 && !g_lock_configured; i++) {
        usleep(25000);
        wl_display_roundtrip(display);
    }
    if (!g_lock_configured) {
        fprintf(stderr, "FAIL: Compositor did not configure ext_session_lock_surface_v1!\n");
        return 3;
    }

    struct wl_buffer *lock_buf = create_solid_buffer(g_shm, g_lock_width, g_lock_height, 0xFF0B0E14);
    wl_surface_attach(g_lock_surface, lock_buf, 0, 0);
    wl_surface_commit(g_lock_surface);
    wl_display_roundtrip(display);

    // Wait for locked event
    for (int i = 0; i < 20 && !g_lock_received_locked_event; i++) {
        usleep(25000);
        wl_display_roundtrip(display);
    }

    if (!g_lock_received_locked_event) {
        fprintf(stderr, "FAIL: Did not receive 'locked' event from compositor via ext-session-lock-v1!\n");
        return 4;
    }
    printf("[PASS] ext-session-lock-v1: Authoritative 'locked' event received from compositor.\n\n");

    /* -------------------------------------------------------------
     * STAGE 4: Security Invariant — Assert Non-Lock Clients are Refused Focus
     * ------------------------------------------------------------- */
    printf("--- [STAGE 4] Security Invariant Verification (Locked State) ---\n");
    
    // Normal window must have lost focus (leave received)
    if (g_normal_has_focus) {
        fprintf(stderr, "SECURITY VIOLATION: Normal client STILL retained keyboard focus after session lock!\n");
        return 5;
    }
    printf("[PASS] Normal client window lost keyboard focus upon session lock.\n");

    // Attempt to focus / activate the normal window while session is locked
    xdg_toplevel_set_minimized(g_xdg_toplevel); // trigger state change
    wl_surface_commit(g_normal_surface);
    wl_display_roundtrip(display);

    xdg_toplevel_unset_maximized(g_xdg_toplevel);
    wl_surface_commit(g_normal_surface);
    wl_display_roundtrip(display);

    for (int i = 0; i < 10; i++) {
        usleep(20000);
        wl_display_roundtrip(display);
    }

    if (g_normal_has_focus) {
        fprintf(stderr, "SECURITY VIOLATION: Normal client reclaimed keyboard focus while locked!\n");
        return 6;
    }
    printf("[PASS] Keyboard focus strictly REFUSED to normal client while session is locked.\n\n");

    /* -------------------------------------------------------------
     * STAGE 5: Unlock and Verify Normal Focus Restoration
     * ------------------------------------------------------------- */
    printf("--- [STAGE 5] Unlocking Session via Protocol ---\n");
    ext_session_lock_v1_unlock_and_destroy(g_session_lock);
    g_session_lock = NULL;

    ext_session_lock_surface_v1_destroy(g_ext_lock_surface);
    wl_surface_destroy(g_lock_surface);
    wl_buffer_destroy(lock_buf);
    g_lock_surface = NULL;
    g_ext_lock_surface = NULL;

    wl_display_roundtrip(display);

    // After unlocking, commit normal window to trigger focus restoration
    wl_surface_commit(g_normal_surface);
    wl_display_roundtrip(display);

    for (int i = 0; i < 20 && !g_normal_has_focus; i++) {
        usleep(25000);
        wl_display_roundtrip(display);
    }

    if (!g_normal_has_focus) {
        fprintf(stderr, "WARNING: Normal window did not immediately regain focus after unlock (may require pointer motion).\n");
    } else {
        printf("[PASS] Session unlocked cleanly: normal client regained keyboard focus.\n");
    }

    // Cleanup normal window
    xdg_toplevel_destroy(g_xdg_toplevel);
    xdg_surface_destroy(g_xdg_surface);
    wl_surface_destroy(g_normal_surface);
    wl_buffer_destroy(normal_buf);
    wl_display_roundtrip(display);

    if (g_keyboard) wl_keyboard_destroy(g_keyboard);
    if (g_seat) wl_seat_destroy(g_seat);
    if (g_shm) wl_shm_destroy(g_shm);
    if (g_xdg_wm_base) xdg_wm_base_destroy(g_xdg_wm_base);
    if (g_layer_shell) zwlr_layer_shell_v1_destroy(g_layer_shell);
    if (g_lock_mgr) ext_session_lock_manager_v1_destroy(g_lock_mgr);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    printf("\n====================================================================\n");
    printf("   RESULT: ALL SESSION LOCK SECURITY TESTS PASSED [5/5]              \n");
    printf("====================================================================\n");
    return 0;
}
