#ifndef TINEXUS_COMP_XDG_TOPLEVEL_NODE_HPP
#define TINEXUS_COMP_XDG_TOPLEVEL_NODE_HPP

#include <string>
#include <cstdint>

namespace tinexus::comp {

struct XdgToplevelState {
    bool activated{false};
    bool maximized{false};
    bool fullscreen{false};
    bool resizing{false};
    bool tiled_left{false};
    bool tiled_right{false};
    bool tiled_top{false};
    bool tiled_bottom{false};
};

class XdgToplevelNode {
public:
    XdgToplevelNode() = default;

    void set_title(const std::string& title) { m_title = title; }
    void set_app_id(const std::string& app_id) { m_app_id = app_id; }
    void set_maximized(bool max) { m_state.maximized = max; }
    void set_fullscreen(bool fs) { m_state.fullscreen = fs; }
    void set_activated(bool act) { m_state.activated = act; }

    [[nodiscard]] const std::string& title() const noexcept { return m_title; }
    [[nodiscard]] const std::string& app_id() const noexcept { return m_app_id; }
    [[nodiscard]] const XdgToplevelState& state() const noexcept { return m_state; }

private:
    std::string m_title{"Untitled Window"};
    std::string m_app_id{"org.tinexus.App"};
    XdgToplevelState m_state;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_XDG_TOPLEVEL_NODE_HPP
