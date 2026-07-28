#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <optional>
#include <cstddef>

struct wl_shm;
struct wl_buffer;

namespace txui::wayland {

class WaylandBuffer final : public NonCopyable {
private:
    uint32* m_data{nullptr};
    wl_buffer* m_buffer{nullptr};
    uint32 m_width{0};
    uint32 m_height{0};
    uint32 m_stride{0};
    std::size_t m_size{0};
    int m_fd{-1};

    WaylandBuffer(uint32* data, wl_buffer* buffer, uint32 width, uint32 height,
                  uint32 stride, std::size_t size, int fd) noexcept;

public:
    WaylandBuffer() = default;
    WaylandBuffer(WaylandBuffer&& other) noexcept;
    WaylandBuffer& operator=(WaylandBuffer&& other) noexcept;
    ~WaylandBuffer() noexcept;

    // Creates an anonymous memfd shared memory buffer and optional wl_buffer (if shm is non-null).
    [[nodiscard]] static std::optional<WaylandBuffer> create(
        wl_shm* shm, uint32 width, uint32 height) noexcept;

    [[nodiscard]] uint32* data() noexcept { return m_data; }
    [[nodiscard]] const uint32* data() const noexcept { return m_data; }
    [[nodiscard]] uint32 width() const noexcept { return m_width; }
    [[nodiscard]] uint32 height() const noexcept { return m_height; }
    [[nodiscard]] uint32 stride() const noexcept { return m_stride; }
    [[nodiscard]] wl_buffer* buffer() const noexcept { return m_buffer; }
    [[nodiscard]] bool is_valid() const noexcept { return m_data != nullptr; }

    void clear(uint32 argb = 0x00000000U) noexcept;
};

} // namespace txui::wayland
