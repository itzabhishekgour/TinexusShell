#pragma once

#include <txui/render/CanvasRenderTarget.hpp>
#include <string_view>
#include <optional>

namespace txui {

class ImageReader {
public:
    // Load a standard uncompressed PNG file created by ImageWriter into a new CanvasRenderTarget.
    // Returns std::nullopt if the file cannot be read or is invalid.
    [[nodiscard]] static std::optional<CanvasRenderTarget> load_png(std::string_view filepath) noexcept;
};

} // namespace txui
