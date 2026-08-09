import os

theme_path = 'include/txui/theme/Theme.hpp'
with open(theme_path, 'r', encoding='utf-8') as f:
    theme_content = f.read()

# Replace the uint32_t definitions with txui::Color
theme_content = theme_content.replace(
    'constexpr uint32_t SURFACE_ELEVATED = 0xFF2A2A2A;',
    'constexpr txui::Color SURFACE_ELEVATED{0x2A, 0x2A, 0x2A, 0xFF};'
)
theme_content = theme_content.replace(
    'constexpr uint32_t ACCENT_COLOR = 0xFF5C88C4;',
    'constexpr txui::Color ACCENT_COLOR{0x5C, 0x88, 0xC4, 0xFF};'
)
theme_content = theme_content.replace(
    'constexpr uint32_t TXT_PRIMARY = 0xFFFFFFFF;',
    'constexpr txui::Color TXT_PRIMARY{0xFF, 0xFF, 0xFF, 0xFF};'
)
theme_content = theme_content.replace(
    'constexpr uint32_t TXT_SECONDARY = 0xFFBBBBBB;',
    'constexpr txui::Color TXT_SECONDARY{0xBB, 0xBB, 0xBB, 0xFF};'
)
theme_content = theme_content.replace(
    'constexpr uint32_t TXT_MUTED = 0xFF777777;',
    'constexpr txui::Color TXT_MUTED{0x77, 0x77, 0x77, 0xFF};'
)

# Need to include Color.hpp in Theme.hpp
if '#include <txui/graphics/Color.hpp>' not in theme_content:
    theme_content = theme_content.replace('#include <cstdint>', '#include <cstdint>\n#include <txui/graphics/Color.hpp>')

with open(theme_path, 'w', encoding='utf-8') as f:
    f.write(theme_content)

