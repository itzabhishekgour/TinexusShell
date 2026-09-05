#pragma once

#include <txui/core/NonCopyable.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>

struct wl_data_device_manager;
struct wl_data_device;
struct wl_data_source;
struct wl_data_offer;
struct wl_display;

namespace txui::wayland {

class WaylandConnection;

class WaylandClipboard final : public NonCopyable {
public:
    explicit WaylandClipboard(WaylandConnection& connection) noexcept;
    ~WaylandClipboard() noexcept;

    // Copies plain text to the Wayland clipboard
    bool set_text(std::string_view text, uint32_t serial = 0) noexcept;

    // Synchronously pastes/reads plain text from the active clipboard selection
    [[nodiscard]] std::string get_text() noexcept;

    // Checks if clipboard currently holds supported text data
    [[nodiscard]] bool has_text() const noexcept;

    // Internal listener callbacks
    void on_data_offer(struct wl_data_offer* offer) noexcept;
    void on_selection(struct wl_data_offer* offer) noexcept;
    void on_offer_mime_type(struct wl_data_offer* offer, const char* mime_type) noexcept;

    void on_source_send(struct wl_data_source* source, const char* mime_type, int32_t fd) noexcept;
    void on_source_cancelled(struct wl_data_source* source) noexcept;

private:
    WaylandConnection& m_conn;
    struct wl_data_device* m_data_device{nullptr};
    struct wl_data_source* m_active_source{nullptr};
    std::string m_current_copy_text;

    struct wl_data_offer* m_current_selection_offer{nullptr};
    std::vector<std::string> m_current_mime_types;
    bool m_has_text{false};

    void setup_device() noexcept;
    void cleanup_source() noexcept;
};

} // namespace txui::wayland
