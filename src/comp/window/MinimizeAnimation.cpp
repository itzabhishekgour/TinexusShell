#include "comp/window/MinimizeAnimation.hpp"
#include "ipcd/protocol/dock_protocol.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "common/logger.hpp"

namespace tinexus::comp {

MinimizeAnimation::MinimizeAnimation(std::shared_ptr<WindowNode> win, float start_opacity, float start_scale, int32_t start_x, int32_t start_y)
    : BaseAnimation(std::chrono::milliseconds(350), AnimationCurve::EaseAccelerate)
    , m_win(win)
    , m_start_opacity(start_opacity)
    , m_start_scale(start_scale)
    , m_start_x(start_x)
    , m_start_y(start_y)
{
}

void MinimizeAnimation::update(double p) {
    if (!m_win) return;
    m_win->opacity = static_cast<float>(std::lerp(m_start_opacity, 0.0f, p));
    m_win->scale   = static_cast<float>(std::lerp(m_start_scale, 0.1f, p));
    m_win->x = static_cast<int32_t>(std::lerp(m_start_x, m_win->dock_icon_x, p));
    m_win->y = static_cast<int32_t>(std::lerp(m_start_y, m_win->dock_icon_y, p));
}

void MinimizeAnimation::on_complete() {
    if (!m_win) return;
    m_win->visible         = false;
    m_win->minimized       = true;
    m_win->opacity         = 1.0f;   // reset for next map
    m_win->scale           = 1.0f;
    m_win->animation_phase = AnimationPhase::None;
    
    // Notify dock
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/user/%d/tinexus/ipc.sock", getuid());
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
#pragma pack(push, 1)
            struct IpcHeader {
                uint32_t magic = 0x544E5853;
                uint16_t version = 0x0100;
                uint16_t msg_type;
                uint16_t flags = 0;
                uint32_t sequence_id = 0;
                uint32_t payload_len = 0;
                uint32_t checksum = 0;
            };
#pragma pack(pop)
            IpcHeader hdr;
            hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_MINIMIZED);
            hdr.payload_len = sizeof(tinexus::ipcd::protocol::DockNotifyPayload);
            
            tinexus::ipcd::protocol::DockNotifyPayload payload{};
            strncpy(payload.app_id, m_win->toplevel.app_id().c_str(), sizeof(payload.app_id) - 1);
            payload.surface_id = m_win->id();

            send(sock, &hdr, sizeof(hdr), MSG_NOSIGNAL);
            send(sock, &payload, sizeof(payload), MSG_NOSIGNAL);
        }
        close(sock);
    }
    
    // Need to reset the active anim. Note that this destroys 'this' if it's the last reference!
    // So we must be careful. We can just schedule reset or let the caller reset it. 
    // Actually, setting to None is enough, the manager will clean it up or the next anim will overwrite.
    // The plan says: win->active_dock_anim.reset();
    // BUT calling reset() on the unique_ptr that owns 'this' inside a method of 'this' is dangerous 
    // since it deletes 'this' while it's executing. We will just leave it, and when the main loop
    // ticks and sees it's not running, it can reset it, or we just rely on animation_phase.
    // Wait, the plan says "win->active_dock_anim.reset();". Let's do it safely:
    auto self_ref = std::move(m_win->active_dock_anim); // take ownership so it doesn't delete us yet
}

} // namespace tinexus::comp
