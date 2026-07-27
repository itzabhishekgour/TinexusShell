#include "comp/backend/backend.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class HeadlessBackend : public Backend {
public:
    bool initialize() override {
        log::info("HeadlessBackend: Initialized headless display backend for CI test suites.");
        return true;
    }
    void poll_events() override {}
    void swap_buffers() override {}
    BackendType type() const noexcept override { return BackendType::Headless; }
};

} // namespace tinexus::comp
