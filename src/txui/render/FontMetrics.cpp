#include <txui/render/FontMetrics.hpp>
#include <txui/render/PixmanBackend.hpp>

namespace txui {

TextExtents FontMetrics::measure(std::string_view text, double font_size,
                                 bool bold, FontFamily family) noexcept {
    return PixmanBackend::measure_text(text, font_size, bold, family);
}

} // namespace txui
