#ifndef TINEXUS_COMP_SHORTCUT_ENGINE_HPP
#define TINEXUS_COMP_SHORTCUT_ENGINE_HPP

#include <string>
#include <functional>
#include <cstdint>

namespace tinexus::comp {

class ShortcutEngine {
public:
    static ShortcutEngine& instance() noexcept;

    ShortcutEngine() = default;
    ~ShortcutEngine() = default;

    using ShortcutCallback = std::function<void(const std::string& shortcut_name)>;

    void set_shortcut_callback(ShortcutCallback cb);
    bool process_key_event(uint32_t modifiers, uint32_t keycode, bool is_pressed, uint32_t keysym = 0);

private:
    ShortcutCallback m_callback{nullptr};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SHORTCUT_ENGINE_HPP
