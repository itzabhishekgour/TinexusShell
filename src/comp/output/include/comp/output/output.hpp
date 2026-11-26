#ifndef TINEXUS_COMP_OUTPUT_OUTPUT_HPP
#define TINEXUS_COMP_OUTPUT_OUTPUT_HPP

#include "comp/surface/blur_manager.hpp"
#include <wayland-server-core.h>
#include <functional>
#include <vector>

struct wlr_output;
struct wlr_allocator;
struct wlr_buffer;
struct wlr_renderer;
struct wlr_scene;
struct wlr_scene_tree;
struct wlr_swapchain;

namespace tinexus::comp {

struct OutputCompositeInfo {
    struct wlr_scene* overlay_scene{nullptr};
    struct wlr_scene_tree* top_layer_tree{nullptr};
    bool fullscreen_active{false};
    bool has_modal_overlay{false};
    bool has_visible_overlay{false};
    std::vector<BlurTargetBox> blur_targets;
    bool has_snap_preview{false};
    BlurTargetBox snap_preview_box{};
};

class IOutputCompositeDelegate {
public:
    virtual ~IOutputCompositeDelegate() = default;
    virtual void query_output_composite_info(struct wlr_output* output, OutputCompositeInfo& out_info) = 0;
    virtual bool has_active_overlay_animations() const = 0;
    virtual void step_overlay_animations(double dt) = 0;
};

class TinexusOutput {
public:
    using DestroyCallback = std::function<void(TinexusOutput*)>;

    TinexusOutput(struct wlr_output* output, struct wlr_allocator* allocator, struct wlr_renderer* renderer, struct wlr_scene* scene);
    ~TinexusOutput();

    bool initialize();
    void enable();
    void disable();
    void set_destroy_callback(DestroyCallback cb);
    void set_composite_delegate(IOutputCompositeDelegate* delegate);
    void mark_overlay_dirty(bool schedule_frame = true);
    static void publish_primary_display_state(const struct wlr_output* output);

    // Core render loop (Direct-Scanout bypass, GLES2 Dual-Kawase blur, or Pixman fallback)
    void frame();

    struct wlr_output* get_wlr_output() const { return m_output; }

private:
    void release_backdrop_resources();

    struct wlr_output* m_output{nullptr};
    struct wlr_allocator* m_allocator{nullptr};
    struct wlr_renderer* m_renderer{nullptr};
    struct wlr_scene* m_scene{nullptr};
    IOutputCompositeDelegate* m_delegate{nullptr};

    struct wlr_swapchain* m_backdrop_swapchain{nullptr};
    struct wlr_buffer* m_last_backdrop_buffer{nullptr};
    bool m_overlay_dirty{false};
    bool m_was_compositing{false};
    std::vector<BlurTargetBox> m_prev_blur_targets;
    OutputCompositeInfo m_composite_info;

    struct timespec m_last_frame_time{0, 0};
    DestroyCallback m_destroy_callback;

    struct wl_listener m_frame_listener{};
    struct wl_listener m_request_state_listener{};
    struct wl_listener m_destroy_listener{};

    static void handle_frame(struct wl_listener* listener, void* data);
    static void handle_request_state(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_OUTPUT_OUTPUT_HPP
