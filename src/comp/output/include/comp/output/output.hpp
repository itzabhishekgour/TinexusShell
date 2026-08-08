#ifndef TINEXUS_COMP_OUTPUT_OUTPUT_HPP
#define TINEXUS_COMP_OUTPUT_OUTPUT_HPP

#include <wayland-server-core.h>

struct wlr_output;
struct wlr_allocator;
struct wlr_renderer;
struct wlr_scene;

namespace tinexus::comp {

class TinexusOutput {
public:
    TinexusOutput(struct wlr_output* output, struct wlr_allocator* allocator, struct wlr_renderer* renderer, struct wlr_scene* scene);
    ~TinexusOutput();

    bool initialize();
    void enable();
    void disable();

    // Core render loop for Phase 2C
    void frame();

    struct wlr_output* get_wlr_output() const { return m_output; }

private:
    struct wlr_output* m_output{nullptr};
    struct wlr_allocator* m_allocator{nullptr};
    struct wlr_renderer* m_renderer{nullptr};
    struct wlr_scene* m_scene{nullptr};
    struct timespec m_last_frame_time{0, 0};

    struct wl_listener m_frame_listener;
    struct wl_listener m_request_state_listener;
    struct wl_listener m_destroy_listener;

    static void handle_frame(struct wl_listener* listener, void* data);
    static void handle_request_state(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_OUTPUT_OUTPUT_HPP
