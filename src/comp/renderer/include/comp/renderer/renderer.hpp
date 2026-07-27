#ifndef TINEXUS_COMP_RENDERER_HPP
#define TINEXUS_COMP_RENDERER_HPP

#include <cstdint>

namespace tinexus::comp {

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool initialize() = 0;
    virtual void begin_frame(uint32_t width, uint32_t height) = 0;
    virtual void end_frame() = 0;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_HPP
