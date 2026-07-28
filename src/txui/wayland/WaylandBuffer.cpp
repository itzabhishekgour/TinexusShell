#include <txui/wayland/WaylandBuffer.hpp>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <utility>
#include <algorithm>

namespace txui::wayland {

WaylandBuffer::WaylandBuffer(uint32* data, wl_buffer* buffer, uint32 width, uint32 height,
                             uint32 stride, std::size_t size, int fd) noexcept
    : m_data(data),
      m_buffer(buffer),
      m_width(width),
      m_height(height),
      m_stride(stride),
      m_size(size),
      m_fd(fd) {}

WaylandBuffer::WaylandBuffer(WaylandBuffer&& other) noexcept
    : m_data(std::exchange(other.m_data, nullptr)),
      m_buffer(std::exchange(other.m_buffer, nullptr)),
      m_width(std::exchange(other.m_width, 0)),
      m_height(std::exchange(other.m_height, 0)),
      m_stride(std::exchange(other.m_stride, 0)),
      m_size(std::exchange(other.m_size, 0)),
      m_fd(std::exchange(other.m_fd, -1)) {}

WaylandBuffer& WaylandBuffer::operator=(WaylandBuffer&& other) noexcept {
    if (this != &other) {
        if (m_buffer != nullptr) wl_buffer_destroy(m_buffer);
        if (m_data != nullptr && m_size > 0) munmap(m_data, m_size);
        if (m_fd >= 0) close(m_fd);

        m_data = std::exchange(other.m_data, nullptr);
        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_width = std::exchange(other.m_width, 0);
        m_height = std::exchange(other.m_height, 0);
        m_stride = std::exchange(other.m_stride, 0);
        m_size = std::exchange(other.m_size, 0);
        m_fd = std::exchange(other.m_fd, -1);
    }
    return *this;
}

WaylandBuffer::~WaylandBuffer() noexcept {
    if (m_buffer != nullptr) wl_buffer_destroy(m_buffer);
    if (m_data != nullptr && m_size > 0) munmap(m_data, m_size);
    if (m_fd >= 0) close(m_fd);
}

std::optional<WaylandBuffer> WaylandBuffer::create(wl_shm* shm, uint32 width, uint32 height) noexcept {
    if (width == 0 || height == 0) {
        return std::nullopt;
    }

    const uint32 stride = width * static_cast<uint32>(sizeof(uint32));
    const std::size_t size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);

    int fd = memfd_create("txui-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return std::nullopt;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        close(fd);
        return std::nullopt;
    }

    void* map_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_ptr == MAP_FAILED) {
        close(fd);
        return std::nullopt;
    }

    uint32* data = static_cast<uint32*>(map_ptr);
    wl_buffer* buffer = nullptr;

    if (shm != nullptr) {
        wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
        if (pool == nullptr) {
            munmap(data, size);
            close(fd);
            return std::nullopt;
        }

        buffer = wl_shm_pool_create_buffer(
            pool, 0,
            static_cast<int32_t>(width),
            static_cast<int32_t>(height),
            static_cast<int32_t>(stride),
            WL_SHM_FORMAT_ARGB8888
        );

        wl_shm_pool_destroy(pool);
        if (buffer == nullptr) {
            munmap(data, size);
            close(fd);
            return std::nullopt;
        }
    }

    return WaylandBuffer(data, buffer, width, height, stride, size, fd);
}

void WaylandBuffer::clear(uint32 argb) noexcept {
    if (m_data != nullptr && m_size > 0) {
        const std::size_t count = m_size / sizeof(uint32);
        std::fill_n(m_data, count, argb);
    }
}

} // namespace txui::wayland
