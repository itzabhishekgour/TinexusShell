#include "comp/animation/animation_manager.hpp"
#include "comp/output/output.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "common/RuntimePaths.hpp"
#include "common/logger.hpp"
#include <ctime>
#include <cmath>
#include <fstream>
#include <filesystem>

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/pass.h>
#define static
#include <wlr/types/wlr_scene.h>
#undef static
}

namespace tinexus::comp {

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
    wl_list_remove(&m_frame_listener.link);
    wl_list_remove(&m_request_state_listener.link);
    wl_list_remove(&m_destroy_listener.link);
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

    // Dynamic Hardware Adaptation: discover active wlr_output mode without hardcoding
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

    // Authoritative runtime state publication for Settings and About UI
    try {
        tinexus::common::RuntimePaths::ensure_runtime_dir();
        std::string run_path = tinexus::common::RuntimePaths::get_runtime_dir() + "/display";
        std::ofstream df(run_path);
        if (df.is_open()) {
            df << "connector=" << (m_output->name ? m_output->name : "unknown") << "\n";
            df << "width=" << mode_w << "\n";
            df << "height=" << mode_h << "\n";
            df << "refresh_mhz=" << refresh_mhz << "\n";
            char hz_buf[32];
            std::snprintf(hz_buf, sizeof(hz_buf), "%.2f", refresh_hz);
            df << "refresh_hz=" << hz_buf << "\n";
            df << "make=" << (m_output->make ? m_output->make : "") << "\n";
            df << "model=" << (m_output->model ? m_output->model : "") << "\n";
        }
        std::filesystem::create_directories("/run/tinexus");
        std::ofstream df_legacy("/run/tinexus/display");
        if (df_legacy.is_open()) {
            df_legacy << "connector=" << (m_output->name ? m_output->name : "unknown") << "\n";
            df_legacy << "width=" << mode_w << "\n";
            df_legacy << "height=" << mode_h << "\n";
            df_legacy << "refresh_mhz=" << refresh_mhz << "\n";
            char hz_buf[32];
            std::snprintf(hz_buf, sizeof(hz_buf), "%.2f", refresh_hz);
            df_legacy << "refresh_hz=" << hz_buf << "\n";
            df_legacy << "make=" << (m_output->make ? m_output->make : "") << "\n";
            df_legacy << "model=" << (m_output->model ? m_output->model : "") << "\n";
        }
    } catch (...) {}

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
    if (!m_scene) return;

    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(m_scene, m_output);
    if (!scene_output) {
        return;
    }

    bool has_anims = AnimationManager::instance().has_active_animations() ||
                     WorkspaceManager::instance().has_active_animation();
    if (!has_anims && !wlr_scene_output_needs_frame(scene_output)) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (m_last_frame_time.tv_sec != 0) {
        double dt = (now.tv_sec - m_last_frame_time.tv_sec) + 
                    (now.tv_nsec - m_last_frame_time.tv_nsec) / 1e9;
        if (dt > 0.0) {
            if (AnimationManager::instance().has_active_animations()) {
                AnimationManager::instance().tick(dt);
            }
            if (WorkspaceManager::instance().has_active_animation()) {
                WorkspaceManager::instance().tick_animation(dt);
            }
        }
    }
    m_last_frame_time = now;

    if (!wlr_scene_output_commit(scene_output, nullptr)) {
        log::error("OutputManager: Failed to commit scene output for '{}'", m_output->name);
    } else {
        static bool first_commit_logged = false;
        if (!first_commit_logged) {
            log::info("OutputManager: First successful scene output commit on '{}'", m_output->name);
            first_commit_logged = true;
        }
    }

    wlr_scene_output_send_frame_done(scene_output, &now);

    if (AnimationManager::instance().has_active_animations() ||
        WorkspaceManager::instance().has_active_animation()) {
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
    log::info("OutputManager: Output '{}' destroyed", self->m_output->name);
}

} // namespace tinexus::comp
