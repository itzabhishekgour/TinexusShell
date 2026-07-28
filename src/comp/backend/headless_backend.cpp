#include "comp/backend/backend.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class HeadlessBackend : public Backend {
public:
    HeadlessBackend() = default;
    ~HeadlessBackend() override = default;

    bool initialize() override {
        log::info("HeadlessBackend: Initializing headless backend");
        return true;
    }

    bool start() override {
        log::info("HeadlessBackend: Starting headless backend");
        return true;
    }

    void stop() override {
        log::info("HeadlessBackend: Stopping headless backend");
    }

    void shutdown() override {
        log::info("HeadlessBackend: Shutting down");
    }

    struct wl_display* display() override {
        return nullptr;
    }

    BackendType type() const noexcept override { return BackendType::Headless; }
};

std::unique_ptr<Backend> create_headless_backend() {
    return std::make_unique<HeadlessBackend>();
}

} // namespace tinexus::comp
