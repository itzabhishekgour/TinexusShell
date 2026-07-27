#ifndef TINEXUS_COMP_SURFACE_TREE_HPP
#define TINEXUS_COMP_SURFACE_TREE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace tinexus::comp {

using WindowID = uint64_t;

struct SurfaceNode {
    WindowID window_id{0};
    std::string title;
    std::string app_id;
    uint32_t width{800};
    uint32_t height{600};
    bool is_toplevel{true};
    std::vector<std::shared_ptr<SurfaceNode>> children;
};

class SurfaceTree {
public:
    SurfaceTree() = default;
    ~SurfaceTree() = default;

    WindowID create_surface(const std::string& app_id, const std::string& title);
    bool destroy_surface(WindowID id);
    [[nodiscard]] size_t active_surface_count() const noexcept;

private:
    WindowID m_next_id{1000};
    std::vector<std::shared_ptr<SurfaceNode>> m_roots;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SURFACE_TREE_HPP
