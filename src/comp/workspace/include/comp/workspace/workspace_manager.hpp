#ifndef TINEXUS_COMP_WORKSPACE_MANAGER_HPP
#define TINEXUS_COMP_WORKSPACE_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace tinexus::comp {

struct Workspace {
    uint32_t id{1};
    std::string name;
    bool is_active{false};
    std::vector<uint64_t> window_ids;
};

class WorkspaceManager {
public:
    static WorkspaceManager& instance() noexcept;

    WorkspaceManager();
    ~WorkspaceManager() = default;

    void initialize_default_workspaces(uint32_t count = 3);
    
    [[nodiscard]] uint32_t active_workspace_id() const noexcept;
    bool switch_workspace(uint32_t workspace_id);
    
    void add_window_to_workspace(uint32_t workspace_id, uint64_t window_id);
    void remove_window_from_workspace(uint64_t window_id);
    
    [[nodiscard]] const std::vector<Workspace>& get_all_workspaces() const noexcept;
    [[nodiscard]] const Workspace* get_workspace(uint32_t workspace_id) const noexcept;

    bool toggle_overview();
    [[nodiscard]] bool is_overview_active() const noexcept { return m_overview_active; }

private:
    struct SavedGeometry {
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
    };

    std::vector<Workspace> m_workspaces;
    uint32_t m_active_id{1};
    std::unordered_map<uint64_t, uint32_t> m_window_to_workspace;
    std::unordered_map<uint64_t, SavedGeometry> m_saved_geometries;
    bool m_overview_active{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WORKSPACE_MANAGER_HPP
