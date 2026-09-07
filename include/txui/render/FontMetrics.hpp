#pragma once

#include <txui/render/Command.hpp>
#include <string_view>

namespace txui {

struct TextExtents {
    double width{0.0};
    double height{0.0};
    double ascent{0.0};
    double descent{0.0};
};

class FontMetrics {
public:
    static TextExtents measure(std::string_view text, double font_size,
                               bool bold = false, FontFamily family = FontFamily::UI) noexcept;
};

} // namespace txui
