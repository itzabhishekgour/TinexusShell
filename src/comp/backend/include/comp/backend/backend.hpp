#ifndef TINEXUS_COMP_BACKEND_HPP
#define TINEXUS_COMP_BACKEND_HPP

#include <string>
#include <memory>

struct wl_display;

namespace tinexus::comp {

enum class BackendType {
    Headless,
    Wlroots,
    X11
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;

    virtual struct wl_display* display() = 0;
    [[nodiscard]] virtual BackendType type() const noexcept = 0;
};

// Factory functions
std::unique_ptr<Backend> create_headless_backend();
std::unique_ptr<Backend> create_wlroots_backend(struct wl_display* display);

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BACKEND_HPP
