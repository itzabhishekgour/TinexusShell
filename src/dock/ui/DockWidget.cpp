#include "dock/DockWidget.hpp"
#include "ipcd/protocol/dock_protocol.hpp"
#include "ipcd/protocol/header.hpp"
#include <txui/render/FontMetrics.hpp>
#include <txui/core/SingleInstance.hpp>
#include <common/RuntimePaths.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace txui {

// ── Design tokens (derived from DockWidget definitions) ────────────────────────
static constexpr double BASE_SIZE       = DockWidget::BASE_SIZE;
static constexpr double GAP             = DockWidget::GAP;
static constexpr double DOCK_PAD        = DockWidget::DOCK_PAD;
static constexpr double DOCK_BOT_MARGIN = DockWidget::DOCK_BOT_MARGIN;
static constexpr double PILL_RADIUS     = DockWidget::PILL_RADIUS;
static constexpr double ICON_RADIUS     = DockWidget::ICON_RADIUS;
static constexpr double MAX_SCALE       = DockWidget::MAX_SCALE;
static constexpr double INFLUENCE_R     = DockWidget::INFLUENCE_R;

DockWidget::DockWidget() {
    m_icons = {
        DockIconState{"tinexus-terminal",  "Terminal",  "tinexus-terminal",    IconType::Terminal},
        DockIconState{"tinexus-files",     "Files",     "tinexus-files",       IconType::Folder},
        DockIconState{"tinexus-settings",  "Settings",  "tinexus-settings-ui", IconType::Gear},
        DockIconState{"tinexus-monitor",   "Monitor",   "tinexus-monitor",     IconType::BarChart},
        DockIconState{"tinexus-store",     "App Store", "tinexus-store",       IconType::Package},
    };
    // Pre-init springs to 1.0 so icons render at correct size from frame 1
    for (auto& icon : m_icons) {
        icon.scale_spring.value  = 1.0;
        icon.scale_spring.target = 1.0;
        icon.bounce_offset_spring.value  = 0.0;
        icon.bounce_offset_spring.target = 0.0;
    }
    setup_ipc();
}

DockWidget::~DockWidget() {
    if (m_ipc_socket >= 0) close(m_ipc_socket);
}

void DockWidget::setup_ipc() {
    m_ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_ipc_socket >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::string sock_path = tinexus::common::RuntimePaths::get_ipc_socket_path();
        strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(m_ipc_socket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            close(m_ipc_socket);
            m_ipc_socket = -1;
        }
    }
}

void DockWidget::send_ipc(uint16_t msg_type, const std::string& app_id) {
    if (m_ipc_socket < 0) return;
    tinexus::ipcd::protocol::Header hdr;
    tinexus::ipcd::protocol::DockNotifyPayload pld;
    hdr.magic       = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    hdr.version     = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    hdr.msg_type    = msg_type;
    hdr.payload_len = sizeof(pld);
    hdr.flags = hdr.sequence_id = hdr.checksum = 0;
    memset(&pld, 0, sizeof(pld));
    strncpy(pld.app_id, app_id.c_str(), sizeof(pld.app_id) - 1);
    pld.surface_id = 0;

    send(m_ipc_socket, &hdr, sizeof(hdr), MSG_NOSIGNAL);
    send(m_ipc_socket, &pld, sizeof(pld), MSG_NOSIGNAL);
}

void DockWidget::spawn_app(const std::string& exec_cmd) {
    // Check single-instance applications before blind fork
    std::string canonical_app_id = txui::get_canonical_app_id(exec_cmd);
    if (txui::is_single_instance_app(canonical_app_id) && txui::SingleInstance::is_app_running(canonical_app_id)) {
        txui::SingleInstance::focus_app(canonical_app_id);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) { execlp(exec_cmd.c_str(), exec_cmd.c_str(), nullptr); exit(1); }
        exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}

void DockWidget::update_icon_state(const std::string& app_id, DockIconAppState state) {
    for (auto& icon : m_icons) {
        if (icon.app_id == app_id) { icon.app_state = state; mark_needs_paint(); break; }
    }
}

void DockWidget::handle_hover(int x, int /*y*/) {
    m_mouse_x = x;
    mark_needs_paint();
}

void DockWidget::reset_hover() {
    m_mouse_x = -1;
    m_hovered_idx = -1;
    mark_needs_paint();
}

bool DockWidget::tick_animations(double dt) {
    bool animating = false;
    for (size_t i = 0; i < m_icons.size(); ++i) {
        auto& icon = m_icons[i];
        double target_scale = 1.0;
        if (m_mouse_x >= 0) {
            double dist = std::abs(m_mouse_x - icon.center_x);
            if (dist < INFLUENCE_R) {
                double norm = dist / INFLUENCE_R;
                double falloff = (std::cos(M_PI * norm) + 1.0) / 2.0;
                target_scale = 1.0 + (MAX_SCALE - 1.0) * falloff;
            }
        }
        icon.scale_spring.target = target_scale;
        bool s = icon.scale_spring.step(dt);
        bool b = icon.bounce_offset_spring.step(dt);
        if (!s || !b) animating = true;
    }
    if (animating) mark_needs_paint();
    return animating;
}

bool DockWidget::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        int lx = event.pointer.x - static_cast<int>(frame().x());
        handle_hover(lx, event.pointer.y - static_cast<int>(frame().y()));
        // Determine hovered icon
        m_hovered_idx = -1;
        for (int i = 0; i < static_cast<int>(m_icons.size()); ++i) {
            auto& icon = m_icons[static_cast<size_t>(i)];
            double scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
            double half  = BASE_SIZE * scale / 2.0;
            if (std::abs(lx - icon.center_x) <= half) { m_hovered_idx = i; break; }
        }
        return true;
    } else if (event.type == EventType::PointerLeave) {
        reset_hover();
        return true;
    } else if (event.type == EventType::PointerButtonPress) {
        int lx = event.pointer.x - static_cast<int>(frame().x());
        for (size_t i = 0; i < m_icons.size(); ++i) {
            auto& icon = m_icons[i];
            double scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
            double half  = BASE_SIZE * scale / 2.0;
            if (std::abs(lx - icon.center_x) <= half) { on_icon_clicked(i); return true; }
        }
    }
    return false;
}

void DockWidget::on_icon_clicked(size_t idx) {
    auto& icon = m_icons[idx];
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
        icon.bounce_offset_spring.reset(0.22, 0.0);
        break;
    }
}

Size DockWidget::measure_override(const Constraints& c) noexcept {
    return Size(c.max_width, 120.0);
}

void DockWidget::paint_override(Painter& p) const noexcept {
    const double gx     = frame().x();
    const double gy     = frame().y();
    const double W      = frame().width();
    const double H      = frame().height();
    const double PILL_H = BASE_SIZE + DOCK_PAD * 2.0;

    // ── Compute pill width ───────────────────────────────────────────────
    double pill_w = DOCK_PAD * 2.0;
    for (auto& icon : m_icons) {
        double scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
        pill_w += BASE_SIZE * scale;
    }
    pill_w += GAP * static_cast<double>(std::max(0, static_cast<int>(m_icons.size()) - 1));

    const double pill_x = gx + (W - pill_w) / 2.0;
    const double pill_y = gy + H - PILL_H - DOCK_BOT_MARGIN;

    // ── Shadow ───────────────────────────────────────────────────────────
    p.fill_rounded_rect({pill_x - 4, pill_y + 10, pill_w + 8, PILL_H},
                        static_cast<int>(PILL_RADIUS), Color{0, 0, 0, 50});
    p.fill_rounded_rect({pill_x - 2, pill_y + 5, pill_w + 4, PILL_H},
                        static_cast<int>(PILL_RADIUS), Color{0, 0, 0, 35});

    // ── Pill border ──────────────────────────────────────────────────────
    p.fill_rounded_rect({pill_x - 1, pill_y - 1, pill_w + 2, PILL_H + 2},
                        static_cast<int>(PILL_RADIUS) + 1, Color{255, 255, 255, 42});

    // ── Pill background (frosted glass) ──────────────────────────────────
    p.fill_gradient_rounded_rect({pill_x, pill_y, pill_w, PILL_H},
                                static_cast<int>(PILL_RADIUS),
                                Color{28, 30, 46, 215},
                                Color{18, 19, 30, 230});

    // ── Draw icons ───────────────────────────────────────────────────────
    double cur_x = pill_x + DOCK_PAD;
    for (auto& icon : m_icons) {
        double scale  = icon.scale_spring.value + icon.bounce_offset_spring.value;
        double size   = BASE_SIZE * scale;
        double half   = size / 2.0;

        icon.center_x = cur_x + half - gx;

        // Icon anchored to pill bottom
        double icon_bot_y = pill_y + PILL_H - DOCK_PAD;
        double icon_top_y = icon_bot_y - size;
        double icon_cx    = cur_x + half;

        // Hover/focused highlight
        bool is_hovered = (m_hovered_idx >= 0 &&
                           icon.app_id == m_icons[static_cast<size_t>(m_hovered_idx)].app_id);
        if (is_hovered) {
            // Subtle bright outer glow
            p.fill_rounded_rect({icon_cx - half - 2.0, icon_top_y - 2.0, size + 4.0, size + 4.0},
                                static_cast<int>(ICON_RADIUS * scale) + 2, Color{255, 255, 255, 40});
        }

        // Canonical zero-overhead vector render: no widget-tree or parent measure bubbling
        Icon::render(p, icon.icon_type, Rect{icon_cx - half, icon_top_y, size, size});

        // ── Running indicator dot ─────────────────────────────────────
        if (icon.app_state != DockIconAppState::NotRunning) {
            double dot_y = pill_y + PILL_H - 3.5;
            if (icon.app_state == DockIconAppState::RunningFocused) {
                // Outer soft halo + bright white core
                p.fill_circle(Point{icon_cx, dot_y}, 4.5, Color{255, 255, 255, 70});
                p.fill_circle(Point{icon_cx, dot_y}, 2.5, Color{255, 255, 255, 255});
            } else if (icon.app_state == DockIconAppState::RunningBg) {
                p.fill_circle(Point{icon_cx, dot_y}, 2.5, Color{255, 255, 255, 150});
            } else if (icon.app_state == DockIconAppState::Minimized) {
                p.fill_circle(Point{icon_cx, dot_y}, 2.0, Color{255, 255, 255, 80});
            }
        }

        cur_x += size + GAP;
    }

    // ── Tooltip label on hover (FontMetrics centered, zero overflow) ──────
    if (m_hovered_idx >= 0 && m_hovered_idx < static_cast<int>(m_icons.size())) {
        const auto& hicon = m_icons[static_cast<size_t>(m_hovered_idx)];
        double scale = hicon.scale_spring.value + hicon.bounce_offset_spring.value;
        double size  = BASE_SIZE * scale;
        double tip_cx = gx + hicon.center_x;
        double cur_icon_top = pill_y + PILL_H - DOCK_PAD - size;

        // Tooltip pill measured precisely via FreeType FontMetrics
        const auto ext = txui::FontMetrics::measure(hicon.label, 12.0, false);
        const double tip_w = ext.width + 20.0;
        const double tip_h = 24.0;
        const double tip_x = tip_cx - tip_w * 0.5;
        const double tip_y = cur_icon_top - tip_h - 6.0;

        p.fill_rounded_rect({tip_x - 1.0, tip_y - 1.0, tip_w + 2.0, tip_h + 2.0},
                            7.0, Color{255, 255, 255, 30});
        p.fill_rounded_rect({tip_x, tip_y, tip_w, tip_h},
                            6.0, Color{20, 20, 35, 220});
        p.draw_text(Point{tip_x + (tip_w - ext.width) * 0.5, tip_y + (tip_h - ext.height) * 0.5},
                    hicon.label, Color{255, 255, 255, 230}, 12.0);
    }
}

} // namespace txui
