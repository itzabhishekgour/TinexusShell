#pragma once

#include <memory>
#include <vector>
#include <cstdint>

namespace tinexus::logo {

struct LogoBuffer {
    std::shared_ptr<std::vector<uint32_t>> pixels;
    uint32_t width{0};
    uint32_t height{0};

    [[nodiscard]] bool is_valid() const noexcept {
        return pixels && !pixels->empty() && width > 0 && height > 0;
    }
};

// Returns cached premultiplied ARGB32 pixels of the official Tinexus logo.
// size <= 32 returns the 32x32 icon (optimized for top bar / menu buttons).
// size > 32 returns the 128x128 / 256x256 icon (for About dialog, Lock screen, etc.).
LogoBuffer get_logo(uint32_t target_size = 128);

} // namespace tinexus::logo
