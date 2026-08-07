#ifndef TXUI_WINDOW_DECORATOR_HPP
#define TXUI_WINDOW_DECORATOR_HPP

#include <txui/window/Window.hpp>
#include <memory>

namespace txui {

// WindowDecorator handles rendering premium system-level aesthetics like
// rounded corners, multi-layered drop shadows (ambient + directional),
// and window outlines/borders.
class WindowDecorator {
public:
    WindowDecorator(Window* window);
    ~WindowDecorator();

    // Sets the corner radius in pixels
    void set_corner_radius(double radius);

    // Sets the shadow intensity (0.0 to 1.0) and spread radius
    void set_shadow_properties(double intensity, double radius, double offset_y);

    // Call this during the window's paint cycle to draw decorations
    // It should typically be drawn BEFORE the window contents (for shadows)
    // and AFTER the window contents (for borders/clipping).
    void pre_paint();
    void post_paint();

private:
    Window* m_window{nullptr};
    double m_corner_radius{12.0};
    double m_shadow_intensity{0.5};
    double m_shadow_radius{32.0};
    double m_shadow_offset_y{16.0};

    // V0.1 implementation details:
    // In a Wayland environment, these decorations typically need to be
    // drawn into the window buffer itself, increasing the buffer size
    // to accommodate the shadow margins.
};

} // namespace txui

#endif // TXUI_WINDOW_DECORATOR_HPP
