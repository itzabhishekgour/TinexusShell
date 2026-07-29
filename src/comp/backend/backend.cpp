#include "comp/backend/backend.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class HeadlessBackend : public Backend {
public:
    bool initialize() override {
        log::info("HeadlessBackend: Initialized headless display backend for CI test suites.");
        return true;
    }
    bool start() override { return true; }
    void stop() override {}
    void shutdown() override {}
    BackendType type() const noexcept override { return BackendType::Headless; }
};

} // namespace tinexus::comp
