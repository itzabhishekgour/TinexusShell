#pragma once

#include <txui/render/Backend.hpp>

namespace txui {

class PixmanBackend final : public Backend {
public:
    PixmanBackend() noexcept = default;
    ~PixmanBackend() override = default;

    void execute(const CommandBuffer& buffer, RenderTarget& target) override;
};

} // namespace txui
