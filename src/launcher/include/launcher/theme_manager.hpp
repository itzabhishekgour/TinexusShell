#ifndef TINEXUS_LAUNCHER_THEME_MANAGER_HPP
#define TINEXUS_LAUNCHER_THEME_MANAGER_HPP

#include <string>

namespace tinexus::launcher {

struct ThemeTokens {
    std::string background_surface{"#13131ACC"}; // 80% opacity surface
    std::string background_input{"#1E1E2A"};
    std::string text_primary{"#F0F0F8"};
    std::string text_secondary{"#9090A8"};
    std::string accent_primary{"#6B8CEF"};       // Indigo-blue
    std::string accent_subtle{"#6B8CEF1A"};
    std::string border_default{"#FFFFFF0F"};
    std::string border_focus{"#6B8CEF80"};
    int corner_radius{12};
    int blur_strength_px{40};
};

class ThemeManager {
public:
    static ThemeManager& instance() noexcept;

    ThemeManager() = default;
    ~ThemeManager() = default;

    [[nodiscard]] ThemeTokens tokens() const noexcept;
    void set_dark_theme();
    void set_light_theme();

private:
    ThemeTokens m_tokens{};
};

} // namespace tinexus::launcher

#endif // TINEXUS_LAUNCHER_THEME_MANAGER_HPP
