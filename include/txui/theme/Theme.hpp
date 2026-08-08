#ifndef TINEXUS_TXUI_THEME_HPP
#define TINEXUS_TXUI_THEME_HPP

#include <cstdint>
#include <txui/graphics/Color.hpp>

namespace txui::theme {

// Shared Layout Constants
constexpr uint32_t PANEL_HEIGHT = 36;
constexpr uint32_t NOTIFICATION_TOP_MARGIN = PANEL_HEIGHT + 16;
constexpr uint32_t NOTIFICATION_RIGHT_MARGIN = 16;


constexpr txui::Color SURFACE_ELEVATED{0x2A, 0x2A, 0x2A, 0xFF};
constexpr txui::Color ACCENT_COLOR{0x5C, 0x88, 0xC4, 0xFF};
constexpr txui::Color TXT_PRIMARY{0xFF, 0xFF, 0xFF, 0xFF};
constexpr txui::Color TXT_SECONDARY{0xBB, 0xBB, 0xBB, 0xFF};
constexpr txui::Color TXT_MUTED{0x77, 0x77, 0x77, 0xFF};
} // namespace txui::theme

#endif // TINEXUS_TXUI_THEME_HPP
