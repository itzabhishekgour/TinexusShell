#pragma once

#include <txui/render/CommandBuffer.hpp>
#include <txui/render/RenderTarget.hpp>
#include <txui/core/NonCopyable.hpp>

namespace txui {

class Backend : public NonCopyable {
public:
    virtual ~Backend() = default;

    // Execute recorded commands against any render target (CanvasRenderTarget, WaylandRenderTarget, etc.)
    virtual void execute(const CommandBuffer& buffer, RenderTarget& target) = 0;
};

} // namespace txui
