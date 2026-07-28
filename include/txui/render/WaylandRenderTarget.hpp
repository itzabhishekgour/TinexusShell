#pragma once

#include <txui/render/RenderTarget.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/wayland/WaylandBuffer.hpp>
#include <txui/wayland/WaylandSurface.hpp>
#include <txui/math/Rect.hpp>
#include <array>
#include <optional>
#include <span>
#include <algorithm>

namespace txui {

class WaylandRenderTarget final : public RenderTarget {
private:
    wayland::WaylandConnection* m_connection{nullptr};
    wayland::WaylandSurface m_surface;
    std::array<wayland::WaylandBuffer, 2> m_buffers;
    std::size_t m_back_index{0};

    WaylandRenderTarget(wayland::WaylandConnection* connection,
                        wayland::WaylandSurface&& surface,
                        wayland::WaylandBuffer&& buf0,
                        wayland::WaylandBuffer&& buf1) noexcept;

public:
    WaylandRenderTarget() = default;
    WaylandRenderTarget(WaylandRenderTarget&& other) noexcept;
    WaylandRenderTarget& operator=(WaylandRenderTarget&& other) noexcept;
    ~WaylandRenderTarget() override = default;

    // Creates a double-buffered WaylandRenderTarget connected to the given WaylandConnection.
    [[nodiscard]] static std::optional<WaylandRenderTarget> create(
        wayland::WaylandConnection& connection, uint32 width, uint32 height) noexcept;

    [[nodiscard]] uint32 width() const noexcept override { return m_buffers[0].width(); }
    [[nodiscard]] uint32 height() const noexcept override { return m_buffers[0].height(); }
    [[nodiscard]] uint32 stride() const noexcept override { return m_buffers[0].stride(); }

    [[nodiscard]] uint32* data() noexcept override { return m_buffers[m_back_index].data(); }
    [[nodiscard]] const uint32* data() const noexcept override { return m_buffers[m_back_index].data(); }

    [[nodiscard]] std::span<uint32> pixels() noexcept override {
        return {data(), static_cast<std::size_t>(width()) * static_cast<std::size_t>(height())};
    }
    [[nodiscard]] std::span<const uint32> pixels() const noexcept override {
        return {data(), static_cast<std::size_t>(width()) * static_cast<std::size_t>(height())};
    }
    void clear(const Color& color = Color::transparent()) noexcept override {
        std::fill_n(data(), static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()),
                    color.to_argb32_premultiplied());
    }
    [[nodiscard]] uint32 pixel_at(uint32 x, uint32 y) const noexcept override {
        if (x >= width() || y >= height()) return 0;
        return data()[static_cast<std::size_t>(y) * width() + x];
    }

    // Attaches the back buffer to the surface, sets damage, commits to Wayland, and swaps front/back buffer.
    void present(const Rect& damage = Rect()) noexcept;

    [[nodiscard]] std::size_t back_index() const noexcept { return m_back_index; }
    [[nodiscard]] std::size_t front_index() const noexcept { return 1 - m_back_index; }
    [[nodiscard]] wayland::WaylandBuffer& buffer(std::size_t idx) noexcept { return m_buffers[idx]; }
    [[nodiscard]] const wayland::WaylandBuffer& buffer(std::size_t idx) const noexcept { return m_buffers[idx]; }
    [[nodiscard]] wayland::WaylandSurface& surface() noexcept { return m_surface; }
    [[nodiscard]] bool is_valid() const noexcept { return m_buffers[0].is_valid() && m_buffers[1].is_valid(); }
};

} // namespace txui
