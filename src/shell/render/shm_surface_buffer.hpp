#pragma once

#include "surface_buffer.hpp"
#include "../client/client_state.hpp"
#include <wayland-client.h>
#include <cstdint>

class ShmSurfaceBuffer : public SurfaceBuffer {
public:
    ShmSurfaceBuffer(ClientState& state, wl_surface* surface, int width, int height);
    ~ShmSurfaceBuffer() override;

    uint32_t* pixels() override { return m_pixels; }
    int width() const override { return m_width; }
    int height() const override { return m_height; }
    void commit() override;

private:
    wl_surface* m_surface;
    wl_buffer* m_buffer{nullptr};
    uint32_t* m_pixels{nullptr};
    int m_width{0};
    int m_height{0};
    int m_fd{-1};
    size_t m_size{0};
};
