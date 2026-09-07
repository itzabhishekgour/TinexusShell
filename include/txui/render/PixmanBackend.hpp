#pragma once

#include <txui/render/Backend.hpp>
#include <txui/render/FontMetrics.hpp>

namespace txui {

class PixmanBackend final : public Backend {
public:
    PixmanBackend() noexcept = default;
    ~PixmanBackend() override = default;

    void execute(const CommandBuffer& buffer, RenderTarget& target) override;

    static TextExtents measure_text(std::string_view text, double font_size,
                                    bool bold = false, FontFamily family = FontFamily::UI) noexcept;
};

} // namespace txui
