#ifndef TINEXUS_COMP_WINDOW_MANAGER_HPP
#define TINEXUS_COMP_WINDOW_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace tinexus::comp {

struct WindowInfo {
    uint64_t window_id{0};
    uint32_t pid{0};
    std::string app_id;
    std::string title;
    int x{0};
    int y{0};
    int width{800};
    int height{600};
    bool is_focused{false};
    bool is_fullscreen{false};
    uint32_t workspace_id{1};
};

class WindowManager {
public:
    static WindowManager& instance() noexcept;

    WindowManager() = default;
    ~WindowManager() = default;

    uint64_t register_window(uint32_t pid, const std::string& app_id, const std::string& title);
    void unregister_window(uint64_t window_id);

    void update_geometry(uint64_t window_id, int x, int y, int width, int height);
    void set_fullscreen(uint64_t window_id, bool fullscreen);

    [[nodiscard]] std::optional<WindowInfo> get_window(uint64_t window_id) const;
    [[nodiscard]] std::vector<WindowInfo> get_all_windows() const;

private:
    std::vector<WindowInfo> m_windows;
    uint64_t m_next_id{1};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_MANAGER_HPP
