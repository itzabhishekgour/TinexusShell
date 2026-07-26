#ifndef TINEXUS_COMP_FOCUS_MANAGER_HPP
#define TINEXUS_COMP_FOCUS_MANAGER_HPP

#include <string>
#include <cstdint>

namespace tinexus::comp {

enum class FocusTargetType : uint8_t {
    Window = 0,
    Launcher = 1,
    Lockscreen = 2,
    Notification = 3,
    PopupMenu = 4
};

inline const char* focus_target_to_string(FocusTargetType type) noexcept {
    switch (type) {
        case FocusTargetType::Window: return "Window";
        case FocusTargetType::Launcher: return "Launcher";
        case FocusTargetType::Lockscreen: return "Lockscreen";
        case FocusTargetType::Notification: return "Notification";
        case FocusTargetType::PopupMenu: return "PopupMenu";
        default: return "Unknown";
    }
}

class FocusManager {
public:
    static FocusManager& instance() noexcept;

    FocusManager() = default;
    ~FocusManager() = default;

    void set_focus(FocusTargetType type, uint64_t surface_id, const std::string& target_id);
    FocusTargetType current_focus_type() const noexcept { return m_focus_type; }
    uint64_t current_surface_id() const noexcept { return m_surface_id; }
    const std::string& current_target_id() const noexcept { return m_target_id; }

private:
    FocusTargetType m_focus_type{FocusTargetType::Window};
    uint64_t m_surface_id{0};
    std::string m_target_id;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_FOCUS_MANAGER_HPP
