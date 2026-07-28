#pragma once

#include <txui/render/RenderTarget.hpp>
#include <txui/core/Assert.hpp>
#include <vector>
#include <algorithm>

namespace txui {

class CanvasRenderTarget final : public RenderTarget {
private:
    uint32 m_width{0};
    uint32 m_height{0};
    std::vector<uint32> m_pixels;

public:
    CanvasRenderTarget() = default;
    CanvasRenderTarget(CanvasRenderTarget&&) noexcept = default;
    CanvasRenderTarget& operator=(CanvasRenderTarget&&) noexcept = default;

    explicit CanvasRenderTarget(uint32 width, uint32 height, const Color& bg_color = Color::transparent())
        : m_width(width),
          m_height(height),
          m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                   bg_color.to_argb32_premultiplied()) {}

    [[nodiscard]] uint32 width() const noexcept override { return m_width; }
    [[nodiscard]] uint32 height() const noexcept override { return m_height; }
    [[nodiscard]] uint32 stride() const noexcept override { return m_width * static_cast<uint32>(sizeof(uint32)); }

    [[nodiscard]] uint32* data() noexcept override { return m_pixels.data(); }
    [[nodiscard]] const uint32* data() const noexcept override { return m_pixels.data(); }

    [[nodiscard]] std::span<uint32> pixels() noexcept override { return m_pixels; }
    [[nodiscard]] std::span<const uint32> pixels() const noexcept override { return m_pixels; }

    void clear(const Color& color = Color::transparent()) noexcept override {
        const uint32 px = color.to_argb32_premultiplied();
        std::fill(m_pixels.begin(), m_pixels.end(), px);
    }

    [[nodiscard]] uint32 pixel_at(uint32 x, uint32 y) const noexcept override {
        TXUI_ASSERT(x < m_width && y < m_height, "CanvasRenderTarget pixel_at out of bounds");
        return m_pixels[static_cast<std::size_t>(y) * m_width + x];
    }
};

} // namespace txui
