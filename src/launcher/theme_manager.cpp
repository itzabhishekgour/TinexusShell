#include "launcher/theme_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::launcher {

ThemeManager& ThemeManager::instance() noexcept {
    static ThemeManager s_instance;
    return s_instance;
}

ThemeTokens ThemeManager::tokens() const noexcept {
    return m_tokens;
}

void ThemeManager::set_dark_theme() {
    m_tokens = ThemeTokens{
        "#13131ACC", // Surface
        "#1E1E2A",   // Input
        "#F0F0F8",   // Text primary
        "#9090A8",   // Text secondary
        "#6B8CEF",   // Accent
        "#6B8CEF1A", // Accent subtle
        "#FFFFFF0F", // Border default
        "#6B8CEF80", // Border focus
        12,
        40
    };
    log::info("ThemeManager: Dark Theme applied");
}

void ThemeManager::set_light_theme() {
    m_tokens = ThemeTokens{
        "#FFFFFFCC", // Surface
        "#F8F8FC",   // Input
        "#0A0A1A",   // Text primary
        "#50506A",   // Text secondary
        "#4A6EE0",   // Accent
        "#4A6EE01A", // Accent subtle
        "#0000000F", // Border default
        "#4A6EE080", // Border focus
        12,
        30
    };
    log::info("ThemeManager: Light Theme applied");
}

} // namespace tinexus::launcher
