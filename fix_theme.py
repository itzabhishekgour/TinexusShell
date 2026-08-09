import os

theme_path = 'include/txui/theme/Theme.hpp'
with open(theme_path, 'r', encoding='utf-8') as f:
    theme_content = f.read()

if 'SURFACE_ELEVATED' not in theme_content:
    insertion = """
constexpr uint32_t SURFACE_ELEVATED = 0xFF2A2A2A;
constexpr uint32_t ACCENT_COLOR = 0xFF5C88C4;
constexpr uint32_t TXT_PRIMARY = 0xFFFFFFFF;
constexpr uint32_t TXT_SECONDARY = 0xFFBBBBBB;
constexpr uint32_t TXT_MUTED = 0xFF777777;
"""
    theme_content = theme_content.replace('} // namespace txui::theme', insertion + '} // namespace txui::theme')
    with open(theme_path, 'w', encoding='utf-8') as f:
        f.write(theme_content)

main_path = 'src/shell/main.cpp'
with open(main_path, 'r', encoding='utf-8') as f:
    main_content = f.read()

main_content = main_content.replace('txui::Theme::', 'txui::theme::')
main_content = main_content.replace('this->bounds()', 'this->frame()')

with open(main_path, 'w', encoding='utf-8') as f:
    f.write(main_content)

