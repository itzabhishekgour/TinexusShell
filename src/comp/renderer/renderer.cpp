#include "comp/renderer/renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class SoftwareRenderer : public Renderer {
public:
    bool initialize() override {
        log::info("SoftwareRenderer: Initialized software compositing renderer.");
        return true;
    }
    void begin_frame(uint32_t width, uint32_t height) override {
        (void)width; (void)height;
    }
    void end_frame() override {}
};

} // namespace tinexus::comp
