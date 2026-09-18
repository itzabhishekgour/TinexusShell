#ifndef TINEXUS_COMP_WORKSPACE_MANAGER_HPP
#define TINEXUS_COMP_WORKSPACE_MANAGER_HPP

#include "comp/animation/animation.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

// Forward declarations
struct wlr_scene_tree;

namespace tinexus::comp {

struct Workspace {
    uint32_t id{1};
    std::string name;
    bool is_active{false};
    std::vector<uint64_t> window_ids;
    struct wlr_scene_tree* scene_tree{nullptr};
};

class WorkspaceManager {
public:
    static WorkspaceManager& instance() noexcept;

    WorkspaceManager();
    ~WorkspaceManager() = default;

    void initialize_default_workspaces(uint32_t count = 9);
    
    [[nodiscard]] uint32_t active_workspace_id() const noexcept;
    bool switch_workspace(uint32_t workspace_id);
    
    void add_window_to_workspace(uint32_t workspace_id, uint64_t window_id);
    void remove_window_from_workspace(uint64_t window_id);
    
    [[nodiscard]] const std::vector<Workspace>& get_all_workspaces() const noexcept;
    [[nodiscard]] const Workspace* get_workspace(uint32_t workspace_id) const noexcept;
    [[nodiscard]] Workspace* get_workspace(uint32_t workspace_id) noexcept;

    // Scene tree accessors
    void set_workspace_scene_tree(uint32_t workspace_id, struct wlr_scene_tree* tree) noexcept;
    [[nodiscard]] struct wlr_scene_tree* get_workspace_scene_tree(uint32_t workspace_id) const noexcept;
    [[nodiscard]] struct wlr_scene_tree* active_scene_tree() const noexcept;

    // Slide animation & viewport control
    void set_viewport_width(uint32_t width) noexcept;
    [[nodiscard]] uint32_t viewport_width() const noexcept { return m_viewport_width; }

    void set_frame_scheduler(std::function<void()> scheduler) noexcept {
        m_frame_scheduler = std::move(scheduler);
    }

    [[nodiscard]] bool has_active_animation() const noexcept { return m_is_sliding; }
    void tick_animation(double dt) noexcept;

    // Gesture-driven interactive scrubbing
    void begin_gesture_swipe() noexcept;
    void update_gesture_swipe(double dx, uint32_t time_msec) noexcept;
    void end_gesture_swipe(bool cancelled) noexcept;
    [[nodiscard]] bool in_gesture() const noexcept { return m_in_gesture; }
    [[nodiscard]] double current_slide_offset() const noexcept { return m_slide_spring.value; }

    bool toggle_overview();
    [[nodiscard]] bool is_overview_active() const noexcept { return m_overview_active; }

private:
    std::vector<Workspace> m_workspaces;
    uint32_t m_active_id{1};
    std::unordered_map<uint64_t, uint32_t> m_window_to_workspace;
    bool m_overview_active{false};

    // Spaces horizontal slide spring physics
    SpringState           m_slide_spring{0.0, 0.0, 0.0};
    bool                  m_is_sliding{false};
    uint32_t              m_viewport_width{1920};
    std::function<void()> m_frame_scheduler;

    // Interactive gesture tracking state
    bool                  m_in_gesture{false};
    uint32_t              m_gesture_start_ws{1};
    double                m_gesture_accum_dx{0.0};
    uint32_t              m_gesture_last_time{0};
    double                m_gesture_velocity{0.0};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WORKSPACE_MANAGER_HPP
