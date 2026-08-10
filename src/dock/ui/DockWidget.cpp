#include "txui/widgets/DockWidget.hpp"
#include "ipcd/protocol/dock_protocol.hpp"
#include "ipcd/protocol/header.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <cstring>

namespace txui {

DockWidget::DockWidget() {
    m_icons = {
        DockIconState{"tinexus-files", "Files", "tinexus-files", IconType::Home},
        DockIconState{"tinexus-settings", "Settings", "tinexus-settings", IconType::Executable},
        DockIconState{"tinexus-launcher", "Search", "tinexus-launcher", IconType::Apps},
        DockIconState{"tinexus-terminal", "Terminal", "alacritty", IconType::Downloads}
    };
    
    setup_ipc();
}

DockWidget::~DockWidget() {
    if (m_ipc_socket >= 0) {
        close(m_ipc_socket);
    }
}

void DockWidget::setup_ipc() {
    m_ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_ipc_socket >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/user/%d/tinexus/ipc.sock", getuid());
        if (connect(m_ipc_socket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            close(m_ipc_socket);
            m_ipc_socket = -1;
        }
    }
}

void DockWidget::send_ipc(uint16_t msg_type, const std::string& app_id) {
    if (m_ipc_socket < 0) return;
    
    struct {
        tinexus::ipcd::protocol::Header hdr;
        tinexus::ipcd::protocol::DockNotifyPayload pld;
    } __attribute__((packed)) msg;
    
    msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    msg.hdr.msg_type = msg_type;
    msg.hdr.payload_len = sizeof(msg.pld);
    msg.hdr.flags = 0;
    msg.hdr.sequence_id = 0;
    msg.hdr.checksum = 0;
    
    memset(&msg.pld, 0, sizeof(msg.pld));
    strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
    msg.pld.surface_id = 0;

    send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
}

void DockWidget::spawn_app(const std::string& exec_cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            execlp(exec_cmd.c_str(), exec_cmd.c_str(), nullptr);
            exit(1);
        }
        exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}

void DockWidget::update_icon_state(const std::string& app_id, DockIconAppState state) {
    for (auto& icon : m_icons) {
        if (icon.app_id == app_id) {
            icon.app_state = state;
            mark_needs_paint();
            break;
        }
    }
}

void DockWidget::handle_hover(int x, int y) {
    m_mouse_x = x;
    mark_needs_paint(); // Triggers recompute of spring targets in tick_animations
}

void DockWidget::reset_hover() {
    m_mouse_x = -1;
    mark_needs_paint();
}

bool DockWidget::tick_animations(double dt) {
    bool animating = false;
    
    const double base_size = 50.0;
    const double max_scale = 1.6;
    const double influence_radius = 2.5 * base_size; // roughly 2-3 icons
    
    for (size_t i = 0; i < m_icons.size(); ++i) {
        auto& icon = m_icons[i];
        
        double target_scale = 1.0;
        if (m_mouse_x >= 0) {
            double dist = std::abs(m_mouse_x - icon.center_x);
            if (dist < influence_radius) {
                // Cosine-like smoothstep for macOS feel
                double normalized_dist = dist / influence_radius;
                // falloff = (cos(pi * dist) + 1) / 2
                double falloff = (std::cos(M_PI * normalized_dist) + 1.0) / 2.0;
                target_scale = 1.0 + (max_scale - 1.0) * falloff;
            }
        }
        
        icon.scale_spring.target = target_scale;
        
        bool scale_settled = icon.scale_spring.step(dt);
        bool bounce_settled = icon.bounce_offset_spring.step(dt);
        
        if (!scale_settled || !bounce_settled) {
            animating = true;
        }
    }
    
    if (animating) {
        mark_needs_paint();
    }
    return animating;
}

bool DockWidget::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        handle_hover(event.pointer.x - frame().x(), event.pointer.y - frame().y());
        return true;
    } else if (event.type == EventType::PointerLeave) {
        reset_hover();
        return true;
    } else if (event.type == EventType::PointerButtonPress) {
        for (size_t i = 0; i < m_icons.size(); ++i) {
            auto& icon = m_icons[i];
            double current_scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
            double size = 50.0 * current_scale;
            double half = size / 2.0;
            if (event.pointer.x - frame().x() >= icon.center_x - half && event.pointer.x - frame().x() <= icon.center_x + half) {
                on_icon_clicked(i);
                return true;
            }
        }
    }
    return false;
}

void DockWidget::on_icon_clicked(size_t icon_idx) {
    auto& icon = m_icons[icon_idx];
    switch (icon.app_state) {
    case DockIconAppState::NotRunning:
        spawn_app(icon.exec);
        icon.bounce_offset_spring.reset(0.4, 0.0);
        break;
    case DockIconAppState::Minimized:
        send_ipc(static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RESTORE_REQUEST), icon.app_id);
        break;
    case DockIconAppState::RunningBg:
        send_ipc(static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_RAISE_AND_FOCUS), icon.app_id);
        break;
    case DockIconAppState::RunningFocused:
        icon.bounce_offset_spring.reset(0.25, 0.0);
        break;
    }
}

Size DockWidget::measure_override(const Constraints& constraints) noexcept {
    return Size(constraints.max_width, 120.0); // Increased height for magnification overflow
}

void DockWidget::paint_override(Painter& p) const noexcept {
    double global_x = frame().x();
    double global_y = frame().y();
    double m_width = frame().width();
    double m_height = frame().height();
    
    // macOS Dock sizing parameters
    const double base_size = 50.0;
    const double gap = 10.0;
    const double dock_margin_bottom = 10.0;
    const double dock_padding = 8.0;
    const double pill_height = base_size + dock_padding * 2.0; // 66.0
    
    // First, compute total width dynamically based on current scales
    double total_pill_width = dock_padding * 2.0;
    for (auto& icon : m_icons) {
        double current_scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
        double size = base_size * current_scale;
        total_pill_width += size;
    }
    total_pill_width += gap * std::max(0, static_cast<int>(m_icons.size()) - 1);
    
    double pill_x = global_x + (m_width - total_pill_width) / 2.0;
    double pill_y = global_y + m_height - pill_height - dock_margin_bottom;
    
    // 1. Draw Drop Shadow for the dock pill (Optimized: single pass)
    p.fill_rounded_rect(Rect{pill_x - 4, pill_y + 8, total_pill_width + 8, pill_height}, 24, Color{0, 0, 0, 40});

    
    // 2. Draw Pill Border (light outer rim)
    p.fill_rounded_rect(Rect{pill_x - 1, pill_y - 1, total_pill_width + 2, pill_height + 2}, 22, Color{255, 255, 255, 40});
    
    // 3. Draw Pill Background (frosted glass approximation)
    p.fill_rounded_rect(Rect{pill_x, pill_y, total_pill_width, pill_height}, 21, Color{30, 30, 30, 140});
    
    // 4. Draw Icons
    double current_x = pill_x + dock_padding;
    for (auto& icon : m_icons) {
        double current_scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
        double size = base_size * current_scale;
        double half = size / 2.0;
        
        // Center for mouse tracking
        icon.center_x = current_x + half - global_x;
        
        // Icons are anchored to the bottom of the pill
        double icon_y_bottom = pill_y + pill_height - dock_padding;
        double icon_y = icon_y_bottom - size;
        
        // Sub-shadow removed for performance
        
        // The App Icon (rounded square)
        // Since we don't have actual image loading in this Painter, we draw beautiful gradients for now
        Color color_top, color_bottom;
        if (icon.app_id == "tinexus-files") { color_top = Color{60, 150, 255, 255}; color_bottom = Color{20, 90, 220, 255}; }
        else if (icon.app_id == "tinexus-settings") { color_top = Color{120, 120, 130, 255}; color_bottom = Color{70, 70, 80, 255}; }
        else if (icon.app_id == "tinexus-launcher") { color_top = Color{255, 100, 100, 255}; color_bottom = Color{200, 40, 40, 255}; }
        else { color_top = Color{50, 200, 100, 255}; color_bottom = Color{10, 140, 60, 255}; }
        
        p.fill_gradient_rounded_rect(Rect{global_x + icon.center_x - half, icon_y, size, size}, 12 * current_scale, color_top, color_bottom);
        
        // Draw the running indicator dot
        if (icon.app_state != DockIconAppState::NotRunning) {
            Color dot_color = (icon.app_state == DockIconAppState::RunningFocused) ? Color{255,255,255,255} : Color{255,255,255,140};
            // Centered below the icon, near the bottom of the pill
            p.fill_circle(Point{global_x + icon.center_x, pill_y + pill_height - 3.5}, 2.0, dot_color);
        }
        
        current_x += size + gap;
    }
}

} // namespace txui

