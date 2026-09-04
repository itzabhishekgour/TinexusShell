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

// ── Design tokens ────────────────────────────────────────────────────────────
static constexpr double BASE_SIZE       = 40.0;
static constexpr double GAP             = 8.0;
static constexpr double DOCK_PAD        = 8.0;
static constexpr double DOCK_BOT_MARGIN = 6.0;
static constexpr double PILL_RADIUS     = 18.0;
static constexpr double ICON_RADIUS     = 10.0;
static constexpr double MAX_SCALE       = 1.55;
static constexpr double INFLUENCE_R     = 2.4 * BASE_SIZE;

DockWidget::DockWidget() {
    m_icons = {
        DockIconState{"tinexus-terminal",  "Terminal",  "tinexus-terminal",  IconType::Downloads},
        DockIconState{"tinexus-files",     "Files",     "tinexus-files",     IconType::Home},
        DockIconState{"tinexus-settings",  "Settings",  "tinexus-settings-ui", IconType::Executable},
        DockIconState{"tinexus-monitor",   "Monitor",   "tinexus-monitor",   IconType::Apps},
        DockIconState{"tinexus-pkg",       "Packages",  "tinexus-pkg",       IconType::Apps},
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
        snprintf(addr.sun_path, sizeof(addr.sun_path),
                 "/run/user/%d/tinexus/ipc.sock", getuid());
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
    msg.hdr.magic       = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    msg.hdr.version     = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    msg.hdr.msg_type    = msg_type;
    msg.hdr.payload_len = sizeof(msg.pld);
    msg.hdr.flags = msg.hdr.sequence_id = msg.hdr.checksum = 0;
    memset(&msg.pld, 0, sizeof(msg.pld));
    strncpy(msg.pld.app_id, app_id.c_str(), sizeof(msg.pld.app_id) - 1);
    msg.pld.surface_id = 0;
    send(m_ipc_socket, &msg, sizeof(msg), MSG_NOSIGNAL);
}

void DockWidget::spawn_app(const std::string& exec_cmd) {
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

// ── Helper: draw icon symbol ─────────────────────────────────────────────────
static void draw_icon_symbol(Painter& p, const std::string& app_id,
                              double cx, double cy, double size) {
    const double s = size;
    const double h = s * 0.5;
    const Color white{255, 255, 255, 230};
    const Color white_dim{255, 255, 255, 150};

    if (app_id == "tinexus-terminal") {
        // >_ symbol
        // ">" chevron
        double chev = s * 0.13;
        double chev_cx = cx - s * 0.10;
        double chev_cy = cy - s * 0.06;
        // draw two lines forming >
        p.fill_rounded_rect({chev_cx - chev*0.4, chev_cy - chev*1.2, chev*1.0, chev*0.28}, 2, white);
        p.fill_rounded_rect({chev_cx - chev*0.4, chev_cy, chev*1.0, chev*0.28}, 2, white);
        // underscore cursor "_"
        p.fill_rounded_rect({cx, cy + s * 0.06, s * 0.22, s * 0.06}, 2, white);

    } else if (app_id == "tinexus-files" || app_id == "tinexus-files-manager") {
        // Folder icon
        double fw = s * 0.56, fh = s * 0.40;
        double fx = cx - fw / 2.0, fy = cy - fh / 2.0 + s * 0.04;
        // Tab on top-left
        p.fill_rounded_rect({fx, fy - s * 0.10, fw * 0.45, s * 0.12}, 3, white);
        // Folder body
        p.fill_rounded_rect({fx, fy, fw, fh}, 5, white);
        // Inner darker rectangle to give folder "open" feel
        p.fill_rounded_rect({fx + fw*0.1, fy + fh*0.25, fw*0.8, fh*0.55}, 3, Color{30, 30, 50, 180});

    } else if (app_id == "tinexus-settings") {
        // Gear/cog icon — draw as circle with tick marks
        double r = s * 0.23;
        p.fill_circle(Point{cx, cy}, r, white);
        // Inner hole
        p.fill_circle(Point{cx, cy}, r * 0.46, Color{60, 60, 80, 255});
        // 8 gear teeth (small rects around)
        for (int tooth = 0; tooth < 8; ++tooth) {
            double angle = tooth * M_PI / 4.0;
            double tx = cx + std::cos(angle) * (r * 1.22);
            double ty = cy + std::sin(angle) * (r * 1.22);
            p.fill_rounded_rect({tx - s*0.04, ty - s*0.04, s*0.09, s*0.09}, 2, white);
        }

    } else if (app_id == "tinexus-monitor") {
        // Bar chart icon
        double bar_w = s * 0.12;
        double base_y = cy + s * 0.22;
        double heights[] = {s*0.22, s*0.36, s*0.28, s*0.44};
        double start_x = cx - (bar_w * 4 + s*0.04 * 3) / 2.0;
        for (int b = 0; b < 4; ++b) {
            double bx = start_x + b * (bar_w + s * 0.04);
            double bh = heights[b];
            Color bc = (b == 3) ? Color{80, 220, 140, 230} : Color{255, 255, 255, 180};
            p.fill_rounded_rect({bx, base_y - bh, bar_w, bh}, 3, bc);
        }

    } else {
        // Generic: package/box icon
        double bw = s * 0.46, bh = s * 0.40;
        double bx = cx - bw/2.0, by = cy - bh/2.0 + s*0.04;
        p.fill_rounded_rect({bx, by, bw, bh}, 6, white_dim);
        // Lid
        p.fill_rounded_rect({bx - s*0.03, by - s*0.10, bw + s*0.06, s*0.13}, 4, white);
        // Center stripe
        p.fill_rounded_rect({cx - s*0.04, by, s*0.08, bh}, 2, Color{60, 60, 80, 180});
    }
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
    p.fill_rounded_rect({pill_x - 2, pill_y + 10, pill_w + 4, PILL_H},
                        static_cast<int>(PILL_RADIUS), Color{0, 0, 0, 35});
    p.fill_rounded_rect({pill_x - 1, pill_y + 6, pill_w + 2, PILL_H},
                        static_cast<int>(PILL_RADIUS), Color{0, 0, 0, 25});

    // ── Pill border ──────────────────────────────────────────────────────
    p.fill_rounded_rect({pill_x - 1, pill_y - 1, pill_w + 2, PILL_H + 2},
                        static_cast<int>(PILL_RADIUS) + 1, Color{255, 255, 255, 35});

    // ── Pill background (frosted glass) ──────────────────────────────────
    p.fill_rounded_rect({pill_x, pill_y, pill_w, PILL_H},
                        static_cast<int>(PILL_RADIUS), Color{22, 22, 35, 185});

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
        double icon_cy    = (icon_top_y + icon_bot_y) / 2.0;

        // ── Icon background square ────────────────────────────────────
        Color bg_top, bg_bot;
        if      (icon.app_id == "tinexus-terminal")  { bg_top = Color{30,  35,  50, 255}; bg_bot = Color{15, 18, 28, 255}; }
        else if (icon.app_id == "tinexus-files")     { bg_top = Color{40, 120, 240, 255}; bg_bot = Color{20, 70, 190, 255}; }
        else if (icon.app_id == "tinexus-settings")  { bg_top = Color{75,  78,  95, 255}; bg_bot = Color{45, 48, 62, 255}; }
        else if (icon.app_id == "tinexus-monitor")   { bg_top = Color{20,  80,  60, 255}; bg_bot = Color{10, 50, 38, 255}; }
        else                                          { bg_top = Color{60,  40, 100, 255}; bg_bot = Color{38, 22, 70, 255}; }

        // Hover/focused highlight
        bool is_hovered = (m_hovered_idx >= 0 &&
                           icon.app_id == m_icons[static_cast<size_t>(m_hovered_idx)].app_id);
        if (is_hovered) {
            // Subtle bright outer glow
            p.fill_rounded_rect({icon_cx - half - 2, icon_top_y - 2, size + 4, size + 4},
                                static_cast<int>(ICON_RADIUS * scale) + 2, Color{255, 255, 255, 30});
        }

        p.fill_gradient_rounded_rect(
            Rect{icon_cx - half, icon_top_y, size, size},
            static_cast<int>(ICON_RADIUS * scale),
            bg_top, bg_bot);

        // ── Symbol ───────────────────────────────────────────────────
        draw_icon_symbol(p, icon.app_id, icon_cx, icon_cy, size);

        // ── Running indicator dot ─────────────────────────────────────
        if (icon.app_state != DockIconAppState::NotRunning) {
            Color dot = (icon.app_state == DockIconAppState::RunningFocused)
                        ? Color{255, 255, 255, 255}
                        : Color{255, 255, 255, 120};
            p.fill_circle(Point{icon_cx, pill_y + PILL_H - 3.0}, 2.5, dot);
        }

        cur_x += size + GAP;
    }

    // ── Tooltip label on hover ────────────────────────────────────────────
    if (m_hovered_idx >= 0 && m_hovered_idx < static_cast<int>(m_icons.size())) {
        const auto& hicon = m_icons[static_cast<size_t>(m_hovered_idx)];
        double scale = hicon.scale_spring.value + hicon.bounce_offset_spring.value;
        double size  = BASE_SIZE * scale;
        double half  = size / 2.0;
        double tip_cx = gx + hicon.center_x;
        double icon_top = pill_y + DOCK_PAD - BASE_SIZE * MAX_SCALE - 2;
        // current icon top
        double cur_icon_top = pill_y + PILL_H - DOCK_PAD - size;

        // Tooltip pill
        double tip_w = static_cast<double>(hicon.label.size()) * 7.5 + 20.0;
        double tip_h = 24.0;
        double tip_x = tip_cx - tip_w / 2.0;
        double tip_y = cur_icon_top - tip_h - 6.0;
        (void)icon_top;

        p.fill_rounded_rect({tip_x - 1, tip_y - 1, tip_w + 2, tip_h + 2},
                            7, Color{255, 255, 255, 30});
        p.fill_rounded_rect({tip_x, tip_y, tip_w, tip_h},
                            6, Color{20, 20, 35, 220});
        p.draw_text(Point{tip_x + 10.0, tip_y + tip_h * 0.5 - 5.5},
                    hicon.label, Color{255, 255, 255, 230}, 12);
    }
}

} // namespace txui
