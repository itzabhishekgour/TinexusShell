#ifndef TINEXUS_COMP_BACKEND_HPP
#define TINEXUS_COMP_BACKEND_HPP

#include <string>

namespace tinexus::comp {

enum class BackendType {
    Headless,
    Wayland,
    Drm
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool initialize() = 0;
    virtual void poll_events() = 0;
    virtual void swap_buffers() = 0;
    [[nodiscard]] virtual BackendType type() const noexcept = 0;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BACKEND_HPP
