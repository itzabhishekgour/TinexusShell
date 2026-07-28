#pragma once

#include <txui/core/Types.hpp>
#include <txui/core/NonCopyable.hpp>
#include <txui/graphics/Color.hpp>
#include <span>

namespace txui {

enum class PixelFormat {
    ARGB8888_Premultiplied // Permanently frozen pixel format for Tinexus UI
};

class RenderTarget : public NonCopyable {
public:
    RenderTarget() = default;
    RenderTarget(RenderTarget&&) noexcept = default;
    RenderTarget& operator=(RenderTarget&&) noexcept = default;
    virtual ~RenderTarget() = default;

    [[nodiscard]] virtual uint32 width() const noexcept = 0;
    [[nodiscard]] virtual uint32 height() const noexcept = 0;
    [[nodiscard]] virtual uint32 stride() const noexcept = 0; // Bytes per row
    [[nodiscard]] virtual PixelFormat format() const noexcept {
        return PixelFormat::ARGB8888_Premultiplied;
    }

    [[nodiscard]] virtual uint32* data() noexcept = 0;
    [[nodiscard]] virtual const uint32* data() const noexcept = 0;
    [[nodiscard]] virtual std::span<uint32> pixels() noexcept = 0;
    [[nodiscard]] virtual std::span<const uint32> pixels() const noexcept = 0;

    virtual void clear(const Color& color = Color::transparent()) noexcept = 0;
    [[nodiscard]] virtual uint32 pixel_at(uint32 x, uint32 y) const noexcept = 0;

    // Deterministic FNV-1a hash of the pixel buffer for Gate 0 replay verification
    [[nodiscard]] virtual uint64 hash_fnv1a() const noexcept {
        const uint8* byte_ptr = reinterpret_cast<const uint8*>(data());
        const std::size_t num_bytes = static_cast<std::size_t>(width()) * height() * sizeof(uint32);
        uint64 hash = 14695981039346656037ULL;
        for (std::size_t i = 0; i < num_bytes; ++i) {
            hash ^= byte_ptr[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

} // namespace txui
