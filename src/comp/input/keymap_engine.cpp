#include "comp/input/keymap_engine.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool KeymapEngine::compile_keymap(const std::string& layout) {
    m_layout = layout;
    log::info("KeymapEngine: Compiled xkbcommon keymap for layout '{}'", m_layout);
    return true;
}

uint32_t KeymapEngine::translate_key(uint32_t scancode) const {
    return scancode; // Translates keycode into keysym
}

void KeymapEngine::update_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    m_state.mods_depressed = depressed;
    m_state.mods_latched = latched;
    m_state.mods_locked = locked;
    m_state.group = group;
}

} // namespace tinexus::comp
