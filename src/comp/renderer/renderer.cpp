#include "comp/renderer/renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class DefaultSoftwareRenderer : public Renderer {
public:
    bool initialize(uint32_t width, uint32_t height) override {
        (void)width; (void)height;
        log::info("DefaultSoftwareRenderer: Initialized software compositing renderer.");
        return true;
    }
    void begin_frame() override {}
    void compose_surface(RenderSurface& surface) override {
        (void)surface;
    }
    void end_frame() override {}
    void present() override {}
};

} // namespace tinexus::comp
