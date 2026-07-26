#ifndef TINEXUS_COMP_CURSOR_MANAGER_HPP
#define TINEXUS_COMP_CURSOR_MANAGER_HPP

#include <string>
#include <cstdint>

namespace tinexus::comp {

struct CursorPosition {
    double x{0.0};
    double y{0.0};
};

class CursorManager {
public:
    static CursorManager& instance() noexcept;

    CursorManager() = default;
    ~CursorManager() = default;

    void set_theme(const std::string& theme_name, uint32_t size = 24);
    void update_position(double x, double y);

    [[nodiscard]] CursorPosition position() const noexcept;
    [[nodiscard]] std::string theme_name() const;

private:
    std::string m_theme{"Adwaita"};
    uint32_t m_size{24};
    CursorPosition m_pos{0.0, 0.0};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_CURSOR_MANAGER_HPP
