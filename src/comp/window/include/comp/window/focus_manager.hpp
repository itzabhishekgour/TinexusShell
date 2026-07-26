#ifndef TINEXUS_COMP_FOCUS_MANAGER_HPP
#define TINEXUS_COMP_FOCUS_MANAGER_HPP

#include <cstdint>
#include <optional>

namespace tinexus::comp {

class FocusManager {
public:
    static FocusManager& instance() noexcept;

    FocusManager() = default;
    ~FocusManager() = default;

    [[nodiscard]] std::optional<uint64_t> focused_window_id() const noexcept;
    void set_focus(uint64_t window_id);
    void clear_focus();

private:
    std::optional<uint64_t> m_focused_id{std::nullopt};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_FOCUS_MANAGER_HPP
