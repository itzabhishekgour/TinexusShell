#include <txui/render/WaylandRenderTarget.hpp>
#include <utility>
#include <algorithm>
#include <cmath>

namespace txui {

WaylandRenderTarget::WaylandRenderTarget(wayland::WaylandConnection* connection,
                                         wayland::WaylandSurface&& surface,
                                         wayland::WaylandBuffer&& buf0,
                                         wayland::WaylandBuffer&& buf1) noexcept
    : m_connection(connection),
      m_surface(std::move(surface)),
      m_buffers{std::move(buf0), std::move(buf1)},
      m_back_index(0) {}

WaylandRenderTarget::WaylandRenderTarget(WaylandRenderTarget&& other) noexcept
    : m_connection(std::exchange(other.m_connection, nullptr)),
      m_surface(std::move(other.m_surface)),
      m_buffers{std::move(other.m_buffers[0]), std::move(other.m_buffers[1])},
      m_back_index(std::exchange(other.m_back_index, 0)) {}

WaylandRenderTarget& WaylandRenderTarget::operator=(WaylandRenderTarget&& other) noexcept {
    if (this != &other) {
        m_connection = std::exchange(other.m_connection, nullptr);
        m_surface = std::move(other.m_surface);
        m_buffers[0] = std::move(other.m_buffers[0]);
        m_buffers[1] = std::move(other.m_buffers[1]);
        m_back_index = std::exchange(other.m_back_index, 0);
    }
    return *this;
}

std::optional<WaylandRenderTarget> WaylandRenderTarget::create(
    wayland::WaylandConnection& connection, uint32 width, uint32 height) noexcept {

    if (!connection.is_valid() || width == 0 || height == 0) {
        return std::nullopt;
    }

    auto surface = wayland::WaylandSurface::create(connection.compositor());
    if (!surface.is_valid()) {
        return std::nullopt;
    }

    auto buf0 = wayland::WaylandBuffer::create(connection.shm(), width, height);
    if (!buf0.has_value() || !buf0->is_valid()) {
        return std::nullopt;
    }

    auto buf1 = wayland::WaylandBuffer::create(connection.shm(), width, height);
    if (!buf1.has_value() || !buf1->is_valid()) {
        return std::nullopt;
    }

    return WaylandRenderTarget(&connection, std::move(surface), std::move(*buf0), std::move(*buf1));
}

void WaylandRenderTarget::present(const Rect& damage) noexcept {
    if (!is_valid()) {
        return;
    }

    // 1. Attach back buffer to Wayland surface
    m_surface.attach(m_buffers[m_back_index], 0, 0);

    // 2. Damage buffer region
    if (damage.is_empty()) {
        m_surface.damage_buffer(0, 0,
                                static_cast<int32>(width()),
                                static_cast<int32>(height()));
    } else {
        int32 x = std::max(0, static_cast<int32>(std::floor(damage.left())));
        int32 y = std::max(0, static_cast<int32>(std::floor(damage.top())));
        int32 w = std::max(1, static_cast<int32>(std::ceil(damage.width())));
        int32 h = std::max(1, static_cast<int32>(std::ceil(damage.height())));
        m_surface.damage_buffer(x, y, w, h);
    }

    // 3. Commit surface
    m_surface.commit();

    // 4. Flush socket connection
    if (m_connection != nullptr) {
        m_connection->flush();
    }

    // 5. Swap front and back buffer indices for double buffering
    m_back_index = 1 - m_back_index;
}

} // namespace txui
