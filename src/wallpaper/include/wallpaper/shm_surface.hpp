#pragma once

#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <memory>
#include <optional>
#include <array>
#include <utility>

namespace tinexus::wallpaper {

class ShmBuffer {
private:
    uint32_t*   m_data{nullptr};
    wl_buffer*  m_buffer{nullptr};
    uint32_t    m_width{0};
    uint32_t    m_height{0};
    uint32_t    m_stride{0};
    size_t      m_size{0};
    int         m_fd{-1};

public:
    ShmBuffer() = default;

    ShmBuffer(uint32_t* data, wl_buffer* buffer, uint32_t width, uint32_t height,
              uint32_t stride, size_t size, int fd) noexcept
        : m_data(data), m_buffer(buffer), m_width(width), m_height(height),
          m_stride(stride), m_size(size), m_fd(fd) {}

    ShmBuffer(const ShmBuffer&) = delete;
    ShmBuffer& operator=(const ShmBuffer&) = delete;

    ShmBuffer(ShmBuffer&& other) noexcept
        : m_data(std::exchange(other.m_data, nullptr)),
          m_buffer(std::exchange(other.m_buffer, nullptr)),
          m_width(std::exchange(other.m_width, 0)),
          m_height(std::exchange(other.m_height, 0)),
          m_stride(std::exchange(other.m_stride, 0)),
          m_size(std::exchange(other.m_size, 0)),
          m_fd(std::exchange(other.m_fd, -1)) {}

    ShmBuffer& operator=(ShmBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            m_data   = std::exchange(other.m_data, nullptr);
            m_buffer = std::exchange(other.m_buffer, nullptr);
            m_width  = std::exchange(other.m_width, 0);
            m_height = std::exchange(other.m_height, 0);
            m_stride = std::exchange(other.m_stride, 0);
            m_size   = std::exchange(other.m_size, 0);
            m_fd     = std::exchange(other.m_fd, -1);
        }
        return *this;
    }

    ~ShmBuffer() noexcept {
        destroy();
    }

    void destroy() noexcept {
        if (m_buffer) {
            wl_buffer_destroy(m_buffer);
            m_buffer = nullptr;
        }
        if (m_data && m_size > 0) {
            munmap(m_data, m_size);
            m_data = nullptr;
        }
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
    }

    [[nodiscard]] static std::optional<ShmBuffer> create(wl_shm* shm, uint32_t width, uint32_t height) noexcept {
        if (!shm || width == 0 || height == 0) return std::nullopt;

        const uint32_t stride = width * 4;
        const size_t size = static_cast<size_t>(stride) * height;

        int fd = memfd_create("tinexus-wallpaper-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (fd < 0) return std::nullopt;

        if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
            close(fd);
            return std::nullopt;
        }

        void* map_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (map_ptr == MAP_FAILED) {
            close(fd);
            return std::nullopt;
        }

        uint32_t* data = static_cast<uint32_t*>(map_ptr);

        wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
        if (!pool) {
            munmap(data, size);
            close(fd);
            return std::nullopt;
        }

        wl_buffer* buffer = wl_shm_pool_create_buffer(
            pool, 0,
            static_cast<int32_t>(width),
            static_cast<int32_t>(height),
            static_cast<int32_t>(stride),
            WL_SHM_FORMAT_ARGB8888
        );
        wl_shm_pool_destroy(pool);

        if (!buffer) {
            munmap(data, size);
            close(fd);
            return std::nullopt;
        }

        return ShmBuffer(data, buffer, width, height, stride, size, fd);
    }

    [[nodiscard]] uint32_t*  data() noexcept { return m_data; }
    [[nodiscard]] const uint32_t* data() const noexcept { return m_data; }
    [[nodiscard]] wl_buffer* buffer() const noexcept { return m_buffer; }
    [[nodiscard]] uint32_t  width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t  height() const noexcept { return m_height; }
    [[nodiscard]] bool      is_valid() const noexcept { return m_data != nullptr && m_buffer != nullptr; }
};

class ShmRenderTarget {
private:
    wl_shm*                   m_shm{nullptr};
    wl_compositor*            m_compositor{nullptr};
    wl_surface*               m_surface{nullptr};
    std::array<ShmBuffer, 2>  m_buffers;
    size_t                    m_back_index{0};
    uint32_t                  m_width{0};
    uint32_t                  m_height{0};

public:
    ShmRenderTarget(wl_shm* shm, wl_compositor* compositor, wl_surface* surface,
                    ShmBuffer&& b0, ShmBuffer&& b1, uint32_t width, uint32_t height) noexcept
        : m_shm(shm), m_compositor(compositor), m_surface(surface),
          m_buffers{std::move(b0), std::move(b1)},
          m_back_index(0), m_width(width), m_height(height) {}

    ShmRenderTarget(const ShmRenderTarget&) = delete;
    ShmRenderTarget& operator=(const ShmRenderTarget&) = delete;

    ShmRenderTarget(ShmRenderTarget&& other) noexcept
        : m_shm(std::exchange(other.m_shm, nullptr)),
          m_compositor(std::exchange(other.m_compositor, nullptr)),
          m_surface(std::exchange(other.m_surface, nullptr)),
          m_buffers{std::move(other.m_buffers[0]), std::move(other.m_buffers[1])},
          m_back_index(std::exchange(other.m_back_index, 0)),
          m_width(std::exchange(other.m_width, 0)),
          m_height(std::exchange(other.m_height, 0)) {}

    ShmRenderTarget& operator=(ShmRenderTarget&& other) noexcept {
        if (this != &other) {
            destroy();
            m_shm        = std::exchange(other.m_shm, nullptr);
            m_compositor = std::exchange(other.m_compositor, nullptr);
            m_surface    = std::exchange(other.m_surface, nullptr);
            m_buffers[0] = std::move(other.m_buffers[0]);
            m_buffers[1] = std::move(other.m_buffers[1]);
            m_back_index = std::exchange(other.m_back_index, 0);
            m_width      = std::exchange(other.m_width, 0);
            m_height     = std::exchange(other.m_height, 0);
        }
        return *this;
    }

    ~ShmRenderTarget() noexcept {
        destroy();
    }

    void destroy() noexcept {
        if (m_surface) {
            wl_surface_destroy(m_surface);
            m_surface = nullptr;
        }
    }

    [[nodiscard]] static std::unique_ptr<ShmRenderTarget> create(
        wl_shm* shm, wl_compositor* compositor, uint32_t width, uint32_t height) noexcept {
        if (!shm || !compositor || width == 0 || height == 0) return nullptr;

        wl_surface* surface = wl_compositor_create_surface(compositor);
        if (!surface) return nullptr;

        auto b0 = ShmBuffer::create(shm, width, height);
        auto b1 = ShmBuffer::create(shm, width, height);
        if (!b0 || !b1) {
            wl_surface_destroy(surface);
            return nullptr;
        }

        return std::make_unique<ShmRenderTarget>(
            shm, compositor, surface, std::move(*b0), std::move(*b1), width, height);
    }

    [[nodiscard]] uint32_t  width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t  height() const noexcept { return m_height; }
    [[nodiscard]] wl_surface* surface() const noexcept { return m_surface; }
    [[nodiscard]] uint32_t* data() noexcept { return m_buffers[m_back_index].data(); }
    [[nodiscard]] const uint32_t* data() const noexcept { return m_buffers[m_back_index].data(); }

    bool resize(uint32_t width, uint32_t height) noexcept {
        if (width == m_width && height == m_height) return true;
        if (!m_shm || width == 0 || height == 0) return false;

        auto b0 = ShmBuffer::create(m_shm, width, height);
        auto b1 = ShmBuffer::create(m_shm, width, height);
        if (!b0 || !b1) return false;

        m_buffers[0] = std::move(*b0);
        m_buffers[1] = std::move(*b1);
        m_width = width;
        m_height = height;
        m_back_index = 0;
        return true;
    }

    void present() noexcept {
        if (!m_surface || !m_buffers[m_back_index].is_valid()) return;

        wl_surface_attach(m_surface, m_buffers[m_back_index].buffer(), 0, 0);
        wl_surface_damage_buffer(m_surface, 0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height));
        wl_surface_commit(m_surface);

        m_back_index = 1 - m_back_index;
    }
};

} // namespace tinexus::wallpaper
