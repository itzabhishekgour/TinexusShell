#include "comp/animation/animation_manager.hpp"
#include "comp/output/output.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "comp/surface/blur_manager.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "common/RuntimePaths.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

extern "C" {
#include <pixman.h>
#include <wlr/render/allocator.h>
#include <wlr/render/gles2.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>
#define static
#include <wlr/types/wlr_scene.h>
#undef static
}

namespace tinexus::comp {

namespace {

bool node_is_descendant_of(struct wlr_scene_node* node, struct wlr_scene_tree* ancestor) {
    if (!node || !ancestor) return false;
    for (struct wlr_scene_node* cur = node; cur != nullptr; cur = (cur->parent ? &cur->parent->node : nullptr)) {
        if (cur == &ancestor->node) return true;
    }
    return false;
}

bool blur_targets_differ(const std::vector<BlurTargetBox>& a, const std::vector<BlurTargetBox>& b) {
    if (a.size() != b.size()) return true;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].x != b[i].x || a[i].y != b[i].y ||
            a[i].width != b[i].width || a[i].height != b[i].height ||
            std::abs(a[i].corner_radius - b[i].corner_radius) > 0.25f ||
            std::abs(a[i].opacity - b[i].opacity) > 0.005f ||
            a[i].radius != b[i].radius ||
            a[i].is_snap_preview != b[i].is_snap_preview) {
            return true;
        }
    }
    return false;
}

struct OverlayRenderCtx {
    struct wlr_output* output{nullptr};
    struct wlr_render_pass* pass{nullptr};
    struct wlr_scene_tree* skip_tree{nullptr};
    struct wlr_box out_box{};
    int32_t fb_w{0};
    int32_t fb_h{0};
    std::vector<struct wlr_scene_buffer*> drawn_buffers;
    std::vector<struct wlr_texture*> drawn_textures;
};

void draw_overlay_buffer_cb(struct wlr_scene_buffer* scene_buf, int sx, int sy, void* data) {
    auto* ctx = static_cast<OverlayRenderCtx*>(data);
    if (!ctx || !scene_buf || !scene_buf->buffer || !ctx->pass) return;
    if (ctx->skip_tree && node_is_descendant_of(&scene_buf->node, ctx->skip_tree)) {
        return;
    }

    const int32_t local_x = sx - ctx->out_box.x;
    const int32_t local_y = sy - ctx->out_box.y;
    const int32_t lw = scene_buf->dst_width > 0 ? scene_buf->dst_width : scene_buf->buffer->width;
    const int32_t lh = scene_buf->dst_height > 0 ? scene_buf->dst_height : scene_buf->buffer->height;
    if (lw <= 0 || lh <= 0) return;

    struct wlr_texture* tex = wlr_texture_from_buffer(ctx->output->renderer, scene_buf->buffer);
    if (!tex) return;

    const float scale = ctx->output->scale > 0.0f ? ctx->output->scale : 1.0f;
    struct wlr_box dst_box{
        static_cast<int>(std::lround(local_x * scale)),
        static_cast<int>(std::lround(local_y * scale)),
        static_cast<int>(std::lround((local_x + lw) * scale)) - static_cast<int>(std::lround(local_x * scale)),
        static_cast<int>(std::lround((local_y + lh) * scale)) - static_cast<int>(std::lround(local_y * scale))
    };
    if (ctx->output->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        wlr_box_transform(&dst_box, &dst_box,
                          wlr_output_transform_invert(ctx->output->transform),
                          ctx->output->width, ctx->output->height);
    }

    const float opacity = scene_buf->opacity;
    struct wlr_render_texture_options tex_opts{};
    tex_opts.texture = tex;
    tex_opts.src_box = scene_buf->src_box;
    tex_opts.dst_box = dst_box;
    tex_opts.alpha = (opacity < 0.999f) ? &opacity : nullptr;
    tex_opts.transform = scene_buf->transform;
    tex_opts.filter_mode = scene_buf->filter_mode;
    tex_opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;

    wlr_render_pass_add_texture(ctx->pass, &tex_opts);
    // Keep overlay textures alive until wlr_render_pass_submit(ov_pass) completes
    ctx->drawn_textures.push_back(tex);
    ctx->drawn_buffers.push_back(scene_buf);
}

} // namespace

TinexusOutput::TinexusOutput(struct wlr_output* output, struct wlr_allocator* allocator, struct wlr_renderer* renderer, struct wlr_scene* scene)
    : m_output(output), m_allocator(allocator), m_renderer(renderer), m_scene(scene) {
    log::info("OutputManager: Detected new output '{}'", m_output->name);

    m_frame_listener.notify = handle_frame;
    wl_signal_add(&m_output->events.frame, &m_frame_listener);

    m_request_state_listener.notify = handle_request_state;
    wl_signal_add(&m_output->events.request_state, &m_request_state_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&m_output->events.destroy, &m_destroy_listener);
}

TinexusOutput::~TinexusOutput() {
    release_backdrop_resources();
    if (m_output) {
        BlurManager::instance().release_output_resources(m_output);
    }
    if (m_frame_listener.link.next) {
        wl_list_remove(&m_frame_listener.link);
        m_frame_listener.link.next = nullptr;
    }
    if (m_request_state_listener.link.next) {
        wl_list_remove(&m_request_state_listener.link);
        m_request_state_listener.link.next = nullptr;
    }
    if (m_destroy_listener.link.next) {
        wl_list_remove(&m_destroy_listener.link);
        m_destroy_listener.link.next = nullptr;
    }
}

void TinexusOutput::release_backdrop_resources() {
    if (m_last_backdrop_buffer) {
        wlr_buffer_unlock(m_last_backdrop_buffer);
        m_last_backdrop_buffer = nullptr;
    }
    if (m_backdrop_swapchain) {
        wlr_swapchain_destroy(m_backdrop_swapchain);
        m_backdrop_swapchain = nullptr;
    }
    m_prev_blur_targets.clear();
}

void TinexusOutput::set_destroy_callback(DestroyCallback cb) {
    m_destroy_callback = std::move(cb);
}

void TinexusOutput::set_composite_delegate(IOutputCompositeDelegate* delegate) {
    m_delegate = delegate;
}

void TinexusOutput::mark_overlay_dirty(bool schedule_frame) {
    m_overlay_dirty = true;
    if (schedule_frame && m_output && m_output->enabled) {
        wlr_output_schedule_frame(m_output);
    }
}

void TinexusOutput::publish_primary_display_state(const struct wlr_output* output) {
    try {
        tinexus::common::RuntimePaths::ensure_runtime_dir();
        const std::string run_path = tinexus::common::RuntimePaths::get_runtime_dir() + "/display";
        std::filesystem::create_directories("/run/tinexus");

        auto write_file = [&](const std::string& path) {
            std::ofstream df(path);
            if (!df.is_open()) return;
            if (!output) {
                df << "connector=none\nwidth=0\nheight=0\nrefresh_mhz=60000\nrefresh_hz=60.00\nmake=\nmodel=\n";
                return;
            }
            int32_t refresh_mhz = 60000;
            int mode_w = output->width;
            int mode_h = output->height;
            if (output->current_mode != nullptr) {
                mode_w = output->current_mode->width;
                mode_h = output->current_mode->height;
                refresh_mhz = output->current_mode->refresh;
            } else if (output->refresh > 0) {
                refresh_mhz = output->refresh;
            }
            double refresh_hz = refresh_mhz > 0 ? (static_cast<double>(refresh_mhz) / 1000.0) : 60.0;
            char hz_buf[32];
            std::snprintf(hz_buf, sizeof(hz_buf), "%.2f", refresh_hz);

            df << "connector=" << (output->name ? output->name : "unknown") << "\n";
            df << "width=" << mode_w << "\n";
            df << "height=" << mode_h << "\n";
            df << "refresh_mhz=" << refresh_mhz << "\n";
            df << "refresh_hz=" << hz_buf << "\n";
            df << "make=" << (output->make ? output->make : "") << "\n";
            df << "model=" << (output->model ? output->model : "") << "\n";
        };

        write_file(run_path);
        write_file("/run/tinexus/display");
    } catch (...) {}
}

bool TinexusOutput::initialize() {
    if (!wlr_output_init_render(m_output, m_allocator, m_renderer)) {
        log::error("OutputManager: Failed to initialize output renderer for '{}'", m_output->name);
        return false;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    struct wlr_output_mode* mode = wlr_output_preferred_mode(m_output);
    if (mode != nullptr) {
        log::info("OutputManager: Setting preferred mode {}x{}@{}mHz for '{}'",
            mode->width, mode->height, mode->refresh, m_output->name);
        wlr_output_state_set_mode(&state, mode);
    } else {
        log::info("OutputManager: No preferred mode for '{}'. Setting custom mode 1280x720@60Hz.", m_output->name);
        wlr_output_state_set_custom_mode(&state, 1280, 720, 60000);
    }

    wlr_output_state_set_enabled(&state, true);

    if (!wlr_output_commit_state(m_output, &state)) {
        log::error("OutputManager: Failed to commit output state for '{}'", m_output->name);
        wlr_output_state_finish(&state);
        return false;
    }

    wlr_output_state_finish(&state);
    log::info("OutputManager: Output '{}' successfully enabled.", m_output->name);

    if (m_output->adaptive_sync_supported &&
        m_output->adaptive_sync_status != WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED) {
        struct wlr_output_state vrr_state;
        wlr_output_state_init(&vrr_state);
        wlr_output_state_set_adaptive_sync_enabled(&vrr_state, true);
        if (wlr_output_commit_state(m_output, &vrr_state)) {
            log::info("OutputManager: Adaptive sync (VRR) enabled for '{}'", m_output->name);
        } else {
            log::debug("OutputManager: Adaptive sync rejected for '{}' (using fixed refresh)", m_output->name);
        }
        wlr_output_state_finish(&vrr_state);
    }

    int32_t refresh_mhz = 60000;
    int mode_w = m_output->width;
    int mode_h = m_output->height;
    if (m_output->current_mode != nullptr) {
        mode_w = m_output->current_mode->width;
        mode_h = m_output->current_mode->height;
        refresh_mhz = m_output->current_mode->refresh;
    } else if (m_output->refresh > 0) {
        refresh_mhz = m_output->refresh;
    } else if (mode != nullptr && mode->refresh > 0) {
        refresh_mhz = mode->refresh;
    }

    double refresh_hz = refresh_mhz > 0 ? (static_cast<double>(refresh_mhz) / 1000.0) : 60.0;
    uint32_t target_hz = static_cast<uint32_t>(std::round(refresh_hz));
    if (target_hz > 0) {
        FrameScheduler::instance().set_target_refresh_rate(target_hz);
    }

    OutputConfig cfg;
    cfg.name = m_output->name ? m_output->name : "Unknown";
    cfg.width = mode_w;
    cfg.height = mode_h;
    cfg.refresh_rate_mhz = refresh_mhz;
    cfg.scale = m_output->scale;
    cfg.enabled = true;
    OutputManager::instance().add_output(cfg);
    if (mode_w > 0) {
        WorkspaceManager::instance().set_viewport_width(static_cast<uint32_t>(mode_w));
    }

    if (OutputManager::instance().get_active_outputs().size() <= 1) {
        publish_primary_display_state(m_output);
    }

    return true;
}

void TinexusOutput::enable() {
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_commit_state(m_output, &state);
    wlr_output_state_finish(&state);
}

void TinexusOutput::disable() {
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, false);
    wlr_output_commit_state(m_output, &state);
    wlr_output_state_finish(&state);
}

void TinexusOutput::frame() {
    if (!m_scene || !m_output) return;

    struct wlr_scene_output* bd_scene_out = wlr_scene_get_scene_output(m_scene, m_output);
    if (!bd_scene_out) {
        return;
    }

    const bool has_anims = AnimationManager::instance().has_active_animations() ||
                           WorkspaceManager::instance().has_active_animation() ||
                           (m_delegate && m_delegate->has_active_overlay_animations());
    const bool bd_needs_frame = wlr_scene_output_needs_frame(bd_scene_out);
    const bool ov_dirty = m_overlay_dirty;
    m_overlay_dirty = false;

    if (!has_anims && !bd_needs_frame && !ov_dirty) {
        m_last_frame_time = {0, 0};
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (m_last_frame_time.tv_sec != 0) {
        double dt = (now.tv_sec - m_last_frame_time.tv_sec) +
                    (now.tv_nsec - m_last_frame_time.tv_nsec) / 1e9;
        dt = std::min(dt, 0.05);
        if (dt > 0.0) {
            if (AnimationManager::instance().has_active_animations()) {
                AnimationManager::instance().tick(dt);
            }
            if (WorkspaceManager::instance().has_active_animation()) {
                WorkspaceManager::instance().tick_animation(dt);
            }
            if (m_delegate && m_delegate->has_active_overlay_animations()) {
                m_delegate->step_overlay_animations(dt);
            }
        }
    }
    m_last_frame_time = now;

    if (m_delegate) {
        m_delegate->query_output_composite_info(m_output, m_composite_info);
    } else {
        m_composite_info = OutputCompositeInfo{};
    }

    const bool fullscreen_scanout_bypass = (m_composite_info.fullscreen_active && !m_composite_info.has_modal_overlay);
    const bool need_composite = !fullscreen_scanout_bypass &&
                                (m_composite_info.has_visible_overlay || !m_composite_info.blur_targets.empty()) &&
                                m_composite_info.overlay_scene != nullptr;

    if (!need_composite) {
        if (m_was_compositing) {
            wlr_damage_ring_add_whole(&bd_scene_out->damage_ring);
            release_backdrop_resources();
            m_was_compositing = false;
        }
        if (!wlr_scene_output_commit(bd_scene_out, nullptr)) {
            log::error("OutputManager: Failed to commit scene output for '{}'", m_output->name);
        } else {
            static bool first_commit_logged = false;
            if (!first_commit_logged) {
                log::info("OutputManager: First successful scene output commit on '{}'", m_output->name);
                first_commit_logged = true;
            }
        }
        wlr_scene_output_send_frame_done(bd_scene_out, &now);
    } else {
        m_was_compositing = true;
        bool committed = false;

        // 1. Update clean unblurred backdrop swapchain only if backdrop changed or resized
        if (bd_needs_frame || !m_last_backdrop_buffer ||
            m_last_backdrop_buffer->width != m_output->width ||
            m_last_backdrop_buffer->height != m_output->height) {

            if (m_last_backdrop_buffer &&
                (m_last_backdrop_buffer->width != m_output->width ||
                 m_last_backdrop_buffer->height != m_output->height)) {
                release_backdrop_resources();
                wlr_damage_ring_add_whole(&bd_scene_out->damage_ring);
            }

            if (wlr_output_configure_primary_swapchain(m_output, nullptr, &m_backdrop_swapchain) && m_backdrop_swapchain) {
                struct wlr_output_state bd_state{};
                wlr_output_state_init(&bd_state);
                struct wlr_scene_output_state_options bd_opts{};
                bd_opts.swapchain = m_backdrop_swapchain;

                if (wlr_scene_output_build_state(bd_scene_out, &bd_state, &bd_opts) && bd_state.buffer) {
                    if (m_last_backdrop_buffer) {
                        wlr_buffer_unlock(m_last_backdrop_buffer);
                        m_last_backdrop_buffer = nullptr;
                    }
                    m_last_backdrop_buffer = wlr_buffer_lock(bd_state.buffer);

                    // Expand backdrop damage ring rects by blur kernel radius (~24px) where touching blur surfaces
                    pixman_region32_t expanded_blur_dmg;
                    pixman_region32_init(&expanded_blur_dmg);
                    bool intersects_blur = false;
                    const float scale = m_output->scale > 0.0f ? m_output->scale : 1.0f;
                    const int32_t pad_px = static_cast<int32_t>(std::ceil(BlurManager::instance().config().radius * scale));
                    if ((bd_state.committed & WLR_OUTPUT_STATE_DAMAGE) && !m_composite_info.blur_targets.empty()) {
                        BlurManager::expand_damage_near_blur(&bd_state.damage,
                                                             m_composite_info.blur_targets,
                                                             pad_px,
                                                             &expanded_blur_dmg,
                                                             &intersects_blur);
                        if (intersects_blur && pixman_region32_not_empty(&expanded_blur_dmg)) {
                            wlr_damage_ring_add(&bd_scene_out->damage_ring, &expanded_blur_dmg);
                        }
                    } else if (!m_composite_info.blur_targets.empty()) {
                        intersects_blur = true;
                    }
                    pixman_region32_fini(&expanded_blur_dmg);

                    const bool geom_changed = blur_targets_differ(m_prev_blur_targets, m_composite_info.blur_targets);
                    if (wlr_renderer_is_gles2(m_renderer) && !m_composite_info.blur_targets.empty()) {
                        BlurManager::instance().prepare_output_blur(
                            m_output, m_renderer, m_last_backdrop_buffer,
                            intersects_blur || geom_changed, m_composite_info.blur_targets);
                    }
                }
                wlr_output_state_finish(&bd_state);
            }
        } else if (wlr_renderer_is_gles2(m_renderer) && !m_composite_info.blur_targets.empty() && m_last_backdrop_buffer) {
            const bool geom_changed = blur_targets_differ(m_prev_blur_targets, m_composite_info.blur_targets);
            if (geom_changed) {
                BlurManager::instance().prepare_output_blur(
                    m_output, m_renderer, m_last_backdrop_buffer, true, m_composite_info.blur_targets);
            }
        }
        m_prev_blur_targets = m_composite_info.blur_targets;

        // 2. Compose Final Frame onto Output Primary Swapchain
        struct wlr_buffer* out_buffer = nullptr;
        if (m_last_backdrop_buffer &&
            wlr_output_configure_primary_swapchain(m_output, nullptr, &m_output->swapchain) &&
            m_output->swapchain) {
            out_buffer = wlr_swapchain_acquire(m_output->swapchain);
        }

        if (out_buffer && m_last_backdrop_buffer) {
            // Pass 2a: Blit cached clean backdrop into out_buffer
            struct wlr_texture* bd_tex = wlr_texture_from_buffer(m_renderer, m_last_backdrop_buffer);
            struct wlr_render_pass* bd_pass = wlr_renderer_begin_buffer_pass(m_renderer, out_buffer, nullptr);
            if (bd_pass && bd_tex) {
                struct wlr_render_texture_options opts{};
                opts.texture = bd_tex;
                opts.dst_box = {0, 0, out_buffer->width, out_buffer->height};
                opts.blend_mode = WLR_RENDER_BLEND_MODE_NONE;
                wlr_render_pass_add_texture(bd_pass, &opts);
                wlr_render_pass_submit(bd_pass);
            } else if (bd_pass) {
                wlr_render_pass_submit(bd_pass);
            }
            if (bd_tex) {
                wlr_texture_destroy(bd_tex);
            }

            // Pass 2b: GLES2 Dual-Kawase Vibrancy + Dithered SDF Blur (or no-op on Pixman)
            bool gles2_blurred = false;
            if (wlr_renderer_is_gles2(m_renderer) && !m_composite_info.blur_targets.empty()) {
                gles2_blurred = BlurManager::instance().composite_output_blur(
                    m_output, m_renderer, out_buffer, m_composite_info.blur_targets);
            }

            // Pass 2c: Draw overlays (Snap Preview Pixman fallback, TopBar, Dock, Launcher, Popups, Cursors)
            OverlayRenderCtx ov_ctx{};
            struct wlr_scene_output* ov_scene_out = wlr_scene_get_scene_output(m_composite_info.overlay_scene, m_output);
            struct wlr_render_pass* ov_pass = wlr_renderer_begin_buffer_pass(m_renderer, out_buffer, nullptr);
            if (ov_pass) {
                if (!gles2_blurred && m_composite_info.has_snap_preview &&
                    m_composite_info.snap_preview_box.width > 0 && m_composite_info.snap_preview_box.height > 0) {
                    // Graceful Tier-4 Pixman fallback: clean frosted translucent tint + border
                    const auto& sb = m_composite_info.snap_preview_box;
                    const float a = std::clamp(sb.opacity, 0.0f, 1.0f);
                    struct wlr_render_rect_options fill_opts{};
                    fill_opts.box = {sb.x, sb.y, sb.width, sb.height};
                    fill_opts.color = {0.32f * 0.24f * a, 0.56f * 0.24f * a, 0.88f * 0.24f * a, 0.24f * a};
                    fill_opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
                    wlr_render_pass_add_rect(ov_pass, &fill_opts);

                    struct wlr_render_rect_options border_opts{};
                    border_opts.color = {0.82f * 0.50f * a, 0.91f * 0.50f * a, 1.0f * 0.50f * a, 0.50f * a};
                    border_opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
                    border_opts.box = {sb.x, sb.y, sb.width, 2};
                    wlr_render_pass_add_rect(ov_pass, &border_opts);
                    border_opts.box = {sb.x, sb.y + sb.height - 2, sb.width, 2};
                    wlr_render_pass_add_rect(ov_pass, &border_opts);
                    border_opts.box = {sb.x, sb.y, 2, sb.height};
                    wlr_render_pass_add_rect(ov_pass, &border_opts);
                    border_opts.box = {sb.x + sb.width - 2, sb.y, 2, sb.height};
                    wlr_render_pass_add_rect(ov_pass, &border_opts);
                }

                if (ov_scene_out) {
                    ov_ctx.output = m_output;
                    ov_ctx.pass = ov_pass;
                    ov_ctx.skip_tree = m_composite_info.fullscreen_active ? m_composite_info.top_layer_tree : nullptr;
                    ov_ctx.out_box = {ov_scene_out->x, ov_scene_out->y, 0, 0};
                    ov_ctx.fb_w = out_buffer->width;
                    ov_ctx.fb_h = out_buffer->height;
                    wlr_scene_output_for_each_buffer(ov_scene_out, draw_overlay_buffer_cb, &ov_ctx);
                }

                wlr_output_add_software_cursors_to_render_pass(m_output, ov_pass, nullptr);
                wlr_render_pass_submit(ov_pass);
            }
            for (auto* tex : ov_ctx.drawn_textures) {
                if (tex) {
                    wlr_texture_destroy(tex);
                }
            }
            ov_ctx.drawn_textures.clear();

            struct wlr_output_state out_state{};
            wlr_output_state_init(&out_state);
            wlr_output_state_set_buffer(&out_state, out_buffer);
            if (wlr_output_commit_state(m_output, &out_state)) {
                committed = true;
            } else {
                log::error("OutputManager: Composite frame commit failed on '{}' — falling back to scene commit", m_output->name);
            }
            wlr_output_state_finish(&out_state);
            wlr_buffer_unlock(out_buffer);

            if (committed) {
                wlr_scene_output_send_frame_done(bd_scene_out, &now);
                for (auto* sb : ov_ctx.drawn_buffers) {
                    wlr_scene_buffer_send_frame_done(sb, &now);
                }
            } else {
                wlr_scene_output_commit(bd_scene_out, nullptr);
                wlr_scene_output_send_frame_done(bd_scene_out, &now);
            }
        } else {
            if (out_buffer) wlr_buffer_unlock(out_buffer);
            wlr_scene_output_commit(bd_scene_out, nullptr);
            wlr_scene_output_send_frame_done(bd_scene_out, &now);
        }
    }

    if (AnimationManager::instance().has_active_animations() ||
        WorkspaceManager::instance().has_active_animation() ||
        (m_delegate && m_delegate->has_active_overlay_animations())) {
        wlr_output_schedule_frame(m_output);
    }
}

void TinexusOutput::handle_frame(struct wl_listener* listener, void* data) {
    (void)data;
    TinexusOutput* self = wl_container_of(listener, self, m_frame_listener);
    self->frame();
}

void TinexusOutput::handle_request_state(struct wl_listener* listener, void* data) {
    TinexusOutput* self = wl_container_of(listener, self, m_request_state_listener);
    auto* event = static_cast<struct wlr_output_event_request_state*>(data);
    wlr_output_commit_state(self->m_output, event->state);
}

void TinexusOutput::handle_destroy(struct wl_listener* listener, void* data) {
    (void)data;
    TinexusOutput* self = wl_container_of(listener, self, m_destroy_listener);
    const std::string out_name = (self->m_output && self->m_output->name) ? self->m_output->name : "";
    log::info("OutputManager: Output '{}' destroyed", out_name.empty() ? "unknown" : out_name);

    self->release_backdrop_resources();
    if (self->m_output) {
        BlurManager::instance().release_output_resources(self->m_output);
    }

    if (!out_name.empty()) {
        OutputManager::instance().remove_output(out_name);
    }

    if (self->m_frame_listener.link.next) {
        wl_list_remove(&self->m_frame_listener.link);
        self->m_frame_listener.link.next = nullptr;
    }
    if (self->m_request_state_listener.link.next) {
        wl_list_remove(&self->m_request_state_listener.link);
        self->m_request_state_listener.link.next = nullptr;
    }
    if (self->m_destroy_listener.link.next) {
        wl_list_remove(&self->m_destroy_listener.link);
        self->m_destroy_listener.link.next = nullptr;
    }

    auto cb = std::move(self->m_destroy_callback);
    if (cb) {
        cb(self);
    }
}

} // namespace tinexus::comp
