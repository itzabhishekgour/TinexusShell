#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <txui/wayland/WaylandBuffer.hpp>

struct wl_surface;
struct wl_compositor;

namespace txui::wayland {

class WaylandSurface final : public NonCopyable {
private:
    wl_surface* m_surface{nullptr};

    explicit WaylandSurface(wl_surface* surface) noexcept;

public:
    WaylandSurface() = default;
    WaylandSurface(WaylandSurface&& other) noexcept;
    WaylandSurface& operator=(WaylandSurface&& other) noexcept;
    ~WaylandSurface() noexcept;

    // Creates a Wayland surface from the given compositor. Returns invalid surface if compositor is nullptr.
    [[nodiscard]] static WaylandSurface create(wl_compositor* compositor) noexcept;

    void attach(WaylandBuffer& buffer, int32 x = 0, int32 y = 0) noexcept;
    void damage_buffer(int32 x, int32 y, int32 width, int32 height) noexcept;
    void commit() noexcept;

    [[nodiscard]] wl_surface* surface() const noexcept { return m_surface; }
    [[nodiscard]] bool is_valid() const noexcept { return m_surface != nullptr; }
};

} // namespace txui::wayland
