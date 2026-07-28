#pragma once

#include <txui/render/RenderTarget.hpp>
#include <string_view>

namespace txui {

class ImageWriter {
public:
    // Write any RenderTarget ARGB32 buffer to a valid PNG image file.
    // Returns true if successfully written, false otherwise.
    [[nodiscard]] static bool save_png(const RenderTarget& target, std::string_view filepath) noexcept;
};

} // namespace txui
