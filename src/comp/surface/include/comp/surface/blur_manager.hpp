// ============================================================================
// blur_manager.hpp — Wayland Blur Protocol + GLES2 Dual-Kawase Pipeline
// Ref: org-kde-kwin-blur.xml & docs/05_UI_UX_GUIDELINES.md §6.1
// ============================================================================
#ifndef TINEXUS_COMP_BLUR_MANAGER_HPP
#define TINEXUS_COMP_BLUR_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <pixman.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include <wayland-server-core.h>
}

struct wlr_buffer;
struct wlr_output;
struct wlr_renderer;
struct wlr_surface;
struct wl_display;
struct wl_resource;
struct wl_listener;

namespace tinexus::comp {

struct BlurConfig {
    int radius{28};
    uint32_t tint{0x13131ACC}; // Standard Tinexus translucent fluid tint (RGBA hex)
    float vibrancy{1.25f};     // Saturation vibrancy boost in upsample composite

    static BlurConfig load_from_config(const std::string& path = "");
};

struct BlurTargetBox {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
    float corner_radius{0.0f};
    float tint_rgba[4]{0.075f, 0.075f, 0.102f, 0.28f};
    float vibrancy{1.25f};
    float opacity{1.0f};
    int32_t radius{28};
    bool is_snap_preview{false};
};

struct BlurSurfaceState {
    struct wlr_surface* surface{nullptr};
    pixman_region32_t region;
    bool has_custom_region{false};
    int radius{28};
    uint32_t tint{0x13131ACC};
    float corner_radius{0.0f};
    struct wl_listener destroy_listener{};
};

class BlurManager {
public:
    static BlurManager& instance();

    bool initialize(struct wl_display* display);
    void shutdown();

    [[nodiscard]] const BlurConfig& config() const noexcept { return m_config; }
    void set_config(const BlurConfig& config);
    void load_config(const std::string& path = "");

    // Protocol handlers
    void register_surface_blur(struct wlr_surface* surface, struct wl_resource* blur_resource);
    void set_surface_blur_region(struct wlr_surface* surface, struct wl_resource* region_resource);
    void set_surface_corner_radius(struct wlr_surface* surface, float corner_radius);
    void commit_surface_blur(struct wlr_surface* surface);
    void unregister_surface_blur(struct wlr_surface* surface);

    // Queries
    bool is_surface_blurred(struct wlr_surface* surface) const;
    const BlurSurfaceState* get_surface_state(struct wlr_surface* surface) const;
    bool is_namespace_blurred(const std::string& ns) const;

    // ── GLES2 Dual-Kawase Offscreen Pipeline (Slice C) ───────────────────────
    bool init_gl(struct wlr_renderer* renderer);
    [[nodiscard]] bool is_gles2_ready() const noexcept;
    void release_output_resources(struct wlr_output* output);

    // Expands backdrop damage by blur kernel radius (~24px) where it touches active blur boxes.
    static void expand_damage_near_blur(const pixman_region32_t* damage,
                                        const std::vector<BlurTargetBox>& targets,
                                        int32_t blur_pad_px,
                                        pixman_region32_t* out_expanded_damage,
                                        bool* out_intersects_blur);

    // Runs Dual-Kawase downsample + upsample FBO ping-pong pyramid only when needed.
    bool prepare_output_blur(struct wlr_output* output,
                             struct wlr_renderer* renderer,
                             struct wlr_buffer* backdrop_buffer,
                             bool backdrop_near_blur_dirty,
                             const std::vector<BlurTargetBox>& targets);

    // Composites blurred, vibrancy-boosted, dithered, SDF-clipped backdrop into dst_buffer FBO.
    bool composite_output_blur(struct wlr_output* output,
                               struct wlr_renderer* renderer,
                               struct wlr_buffer* dst_buffer,
                               const std::vector<BlurTargetBox>& targets);

    // Diagnostic telemetry for resource leak verification
    [[nodiscard]] size_t active_gpu_output_count() const noexcept;

private:
    BlurManager();
    ~BlurManager();

    BlurManager(const BlurManager&) = delete;
    BlurManager& operator=(const BlurManager&) = delete;

    struct GLImpl;
    std::unique_ptr<GLImpl> m_gl;

    struct wl_global* m_global{nullptr};
    struct wl_display* m_display{nullptr};

    BlurConfig m_config;
    std::unordered_map<struct wlr_surface*, std::unique_ptr<BlurSurfaceState>> m_surfaces;
    std::unordered_set<std::string> m_blurred_namespaces{
        "topbar", "tinexus-shell", "tinexus-panel",
        "launcher", "tinexus-launcher",
        "dock", "tinexus-dock", "dock-menu", "dock-stacks",
        "notifications", "tinexus-notifications"
    };
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BLUR_MANAGER_HPP
