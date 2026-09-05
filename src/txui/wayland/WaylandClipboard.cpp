#include <txui/wayland/WaylandClipboard.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <wayland-client.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>

namespace txui::wayland {

namespace {

// ── Data Offer Listener ──────────────────────────────────────────────────────
void data_offer_offer(void* data, struct wl_data_offer* offer, const char* mime_type) {
    auto* self = static_cast<WaylandClipboard*>(data);
    if (self && mime_type) {
        self->on_offer_mime_type(offer, mime_type);
    }
}

void data_offer_source_actions(void* /*data*/, struct wl_data_offer* /*offer*/, uint32_t /*actions*/) {}
void data_offer_action(void* /*data*/, struct wl_data_offer* /*offer*/, uint32_t /*action*/) {}

const struct wl_data_offer_listener s_data_offer_listener = {
    .offer = data_offer_offer,
    .source_actions = data_offer_source_actions,
    .action = data_offer_action,
};

// ── Data Device Listener ─────────────────────────────────────────────────────
void data_device_data_offer(void* data, struct wl_data_device* /*device*/, struct wl_data_offer* offer) {
    auto* self = static_cast<WaylandClipboard*>(data);
    if (self && offer) {
        wl_data_offer_add_listener(offer, &s_data_offer_listener, self);
        self->on_data_offer(offer);
    }
}

void data_device_enter(void* /*data*/, struct wl_data_device* /*device*/, uint32_t /*serial*/,
                       struct wl_surface* /*surface*/, wl_fixed_t /*x*/, wl_fixed_t /*y*/,
                       struct wl_data_offer* /*offer*/) {}
void data_device_leave(void* /*data*/, struct wl_data_device* /*device*/) {}
void data_device_motion(void* /*data*/, struct wl_data_device* /*device*/, uint32_t /*time*/,
                        wl_fixed_t /*x*/, wl_fixed_t /*y*/) {}
void data_device_drop(void* /*data*/, struct wl_data_device* /*device*/) {}

void data_device_selection(void* data, struct wl_data_device* /*device*/, struct wl_data_offer* offer) {
    auto* self = static_cast<WaylandClipboard*>(data);
    if (self) {
        self->on_selection(offer);
    }
}

const struct wl_data_device_listener s_data_device_listener = {
    .data_offer = data_device_data_offer,
    .enter = data_device_enter,
    .leave = data_device_leave,
    .motion = data_device_motion,
    .drop = data_device_drop,
    .selection = data_device_selection,
};

// ── Data Source Listener ─────────────────────────────────────────────────────
void data_source_target(void* /*data*/, struct wl_data_source* /*source*/, const char* /*mime_type*/) {}

void data_source_send(void* data, struct wl_data_source* source, const char* mime_type, int32_t fd) {
    auto* self = static_cast<WaylandClipboard*>(data);
    if (self) {
        self->on_source_send(source, mime_type, fd);
    } else {
        close(fd);
    }
}

void data_source_cancelled(void* data, struct wl_data_source* source) {
    auto* self = static_cast<WaylandClipboard*>(data);
    if (self) {
        self->on_source_cancelled(source);
    }
}

void data_source_dnd_drop_performed(void* /*data*/, struct wl_data_source* /*source*/) {}
void data_source_dnd_finished(void* /*data*/, struct wl_data_source* /*source*/) {}
void data_source_action(void* /*data*/, struct wl_data_source* /*source*/, uint32_t /*action*/) {}

const struct wl_data_source_listener s_data_source_listener = {
    .target = data_source_target,
    .send = data_source_send,
    .cancelled = data_source_cancelled,
    .dnd_drop_performed = data_source_dnd_drop_performed,
    .dnd_finished = data_source_dnd_finished,
    .action = data_source_action,
};

} // namespace

WaylandClipboard::WaylandClipboard(WaylandConnection& connection) noexcept
    : m_conn(connection) {
    setup_device();
}

WaylandClipboard::~WaylandClipboard() noexcept {
    cleanup_source();
    if (m_current_selection_offer) {
        wl_data_offer_destroy(m_current_selection_offer);
        m_current_selection_offer = nullptr;
    }
    if (m_data_device) {
        wl_data_device_destroy(m_data_device);
        m_data_device = nullptr;
    }
}

void WaylandClipboard::setup_device() noexcept {
    if (!m_conn.data_device_manager() || !m_conn.seat()) {
        return;
    }
    m_data_device = wl_data_device_manager_get_data_device(
        m_conn.data_device_manager(), m_conn.seat());
    if (m_data_device) {
        wl_data_device_add_listener(m_data_device, &s_data_device_listener, this);
        m_conn.roundtrip();
    }
}

void WaylandClipboard::cleanup_source() noexcept {
    if (m_active_source) {
        wl_data_source_destroy(m_active_source);
        m_active_source = nullptr;
    }
}

bool WaylandClipboard::set_text(std::string_view text, uint32_t serial) noexcept {
    m_current_copy_text = std::string(text);

    if (!m_conn.data_device_manager() || !m_data_device) {
        setup_device();
        if (!m_data_device) return false;
    }

    cleanup_source();

    m_active_source = wl_data_device_manager_create_data_source(m_conn.data_device_manager());
    if (!m_active_source) {
        return false;
    }

    wl_data_source_add_listener(m_active_source, &s_data_source_listener, this);
    wl_data_source_offer(m_active_source, "text/plain;charset=utf-8");
    wl_data_source_offer(m_active_source, "text/plain");
    wl_data_source_offer(m_active_source, "UTF8_STRING");

    wl_data_device_set_selection(m_data_device, m_active_source, serial);
    m_conn.flush();
    return true;
}

std::string WaylandClipboard::get_text() noexcept {
    if (!m_data_device) {
        setup_device();
    }
    m_conn.roundtrip();

    if (!m_current_selection_offer) {
        return m_current_copy_text; // Fallback to process local clipboard if no remote selection
    }

    // Determine compatible mime type
    std::string chosen_mime;
    for (const auto& mime : m_current_mime_types) {
        if (mime == "text/plain;charset=utf-8" || mime == "text/plain" || mime == "UTF8_STRING") {
            chosen_mime = mime;
            break;
        }
    }

    if (chosen_mime.empty()) {
        return "";
    }

    int pipefds[2];
    if (pipe2(pipefds, O_CLOEXEC) != 0) {
        return "";
    }

    wl_data_offer_receive(m_current_selection_offer, chosen_mime.c_str(), pipefds[1]);
    close(pipefds[1]); // Close write end in parent process so read will encounter EOF
    m_conn.flush();
    m_conn.roundtrip();

    std::string result;
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefds[0], buffer, sizeof(buffer))) > 0) {
        result.append(buffer, static_cast<size_t>(bytes_read));
    }
    close(pipefds[0]);

    return result;
}

bool WaylandClipboard::has_text() const noexcept {
    if (!m_current_selection_offer) {
        return !m_current_copy_text.empty();
    }
    return m_has_text;
}

void WaylandClipboard::on_data_offer(struct wl_data_offer* offer) noexcept {
    // A new offer is being constructed by Wayland compositor
    (void)offer;
}

void WaylandClipboard::on_offer_mime_type(struct wl_data_offer* offer, const char* mime_type) noexcept {
    if (offer == m_current_selection_offer) {
        m_current_mime_types.emplace_back(mime_type);
        if (std::strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
            std::strcmp(mime_type, "text/plain") == 0 ||
            std::strcmp(mime_type, "UTF8_STRING") == 0) {
            m_has_text = true;
        }
    }
}

void WaylandClipboard::on_selection(struct wl_data_offer* offer) noexcept {
    if (m_current_selection_offer && m_current_selection_offer != offer) {
        wl_data_offer_destroy(m_current_selection_offer);
    }
    m_current_selection_offer = offer;
    m_current_mime_types.clear();
    m_has_text = false;
}

void WaylandClipboard::on_source_send(struct wl_data_source* /*source*/, const char* /*mime_type*/, int32_t fd) noexcept {
    if (fd < 0) return;

    // Stream raw UTF-8 text into pipe fd
    const char* ptr = m_current_copy_text.data();
    size_t remaining = m_current_copy_text.size();

    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);
        if (written <= 0) {
            break;
        }
        ptr += written;
        remaining -= static_cast<size_t>(written);
    }

    close(fd);
}

void WaylandClipboard::on_source_cancelled(struct wl_data_source* source) noexcept {
    if (source == m_active_source) {
        wl_data_source_destroy(m_active_source);
        m_active_source = nullptr;
    }
}

} // namespace txui::wayland
