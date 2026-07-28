#include "shm_surface_buffer.hpp"
#include "common/logger.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

static void buffer_release(void* data, struct wl_buffer* buffer) {
    // We could track if the buffer is busy here
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static int allocate_shm_file(size_t size) {
    int fd = memfd_create("tinexus-wayland-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return -1;
    }
    
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    
    if (ret < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

ShmSurfaceBuffer::ShmSurfaceBuffer(ClientState& state, wl_surface* surface, int width, int height)
    : m_surface(surface), m_width(width), m_height(height) {
    
    int stride = width * 4;
    m_size = stride * height;

    m_fd = allocate_shm_file(m_size);
    if (m_fd < 0) {
        tinexus::log::error("Failed to allocate SHM file");
        return;
    }

    void* data = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (data == MAP_FAILED) {
        tinexus::log::error("Failed to mmap SHM file");
        close(m_fd);
        m_fd = -1;
        return;
    }

    m_pixels = static_cast<uint32_t*>(data);
    std::memset(m_pixels, 0, m_size);

    struct wl_shm_pool* pool = wl_shm_create_pool(state.shm, m_fd, m_size);
    m_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_buffer_add_listener(m_buffer, &buffer_listener, this);
    wl_shm_pool_destroy(pool);
}

ShmSurfaceBuffer::~ShmSurfaceBuffer() {
    if (m_buffer) wl_buffer_destroy(m_buffer);
    if (m_pixels) munmap(m_pixels, m_size);
    if (m_fd >= 0) close(m_fd);
}

void ShmSurfaceBuffer::commit() {
    if (!m_surface || !m_buffer) return;
    
    // Attach the buffer to the surface and damage the whole surface
    wl_surface_attach(m_surface, m_buffer, 0, 0);
    wl_surface_damage_buffer(m_surface, 0, 0, m_width, m_height);
    wl_surface_commit(m_surface);
}
