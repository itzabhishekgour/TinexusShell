#ifndef TINEXUS_COMP_KEYMAP_ENGINE_HPP
#define TINEXUS_COMP_KEYMAP_ENGINE_HPP

#include <string>
#include <cstdint>

namespace tinexus::comp {

struct KeymapState {
    uint32_t mods_depressed{0};
    uint32_t mods_latched{0};
    uint32_t mods_locked{0};
    uint32_t group{0};
};

class KeymapEngine {
public:
    KeymapEngine() = default;
    ~KeymapEngine() = default;

    bool compile_keymap(const std::string& layout = "us");
    uint32_t translate_key(uint32_t scancode) const;
    void update_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    [[nodiscard]] const KeymapState& state() const noexcept { return m_state; }

private:
    std::string m_layout{"us"};
    KeymapState m_state;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_KEYMAP_ENGINE_HPP
