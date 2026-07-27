#ifndef TINEXUS_COMP_SURFACE_STATE_HPP
#define TINEXUS_COMP_SURFACE_STATE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

struct wl_resource;

namespace tinexus::comp {

using WindowID = uint64_t;

struct Geometry {
    int32_t x{0};
    int32_t y{0};
    uint32_t width{800};
    uint32_t height{600};
};

struct BufferState {
    void* data{nullptr};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};
    uint32_t format{0};
};

class SurfaceState {
public:
    SurfaceState() = default;
    SurfaceState(WindowID id, const std::string& app_id = "default");
    ~SurfaceState() = default;

    WindowID id{0};
    std::string app_id{"default"};
    std::string title{"Untitled"};

    struct wl_resource* wl_surface{nullptr};
    struct wl_resource* xdg_surface{nullptr};
    struct wl_resource* xdg_toplevel{nullptr};

    std::shared_ptr<BufferState> pending_buffer;
    std::shared_ptr<BufferState> current_buffer;

    Geometry geometry;
    bool mapped{false};
    bool configured{false};
    uint32_t last_configure_serial{0};

    void commit_pending();
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SURFACE_STATE_HPP
