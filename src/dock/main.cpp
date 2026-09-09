#include "txui/window/Window.hpp"
#include "dock/DockWidget.hpp"
#include "ipcd/protocol/dock_protocol.hpp"
#include "ipcd/protocol/header.hpp"
#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <cstring>
#include <vector>

int main() {
    tinexus::log::set_component_name("dock");
    tinexus::log::info("tinexus-dock starting...");

    auto window = txui::Window::create(1920, 120, "dock", true); // true = layer shell
    
    window->set_layer_shell_config(
        txui::LayerType::Bottom, 
        txui::LayerAnchor::Bottom | txui::LayerAnchor::Left | txui::LayerAnchor::Right, 
        96 // exclusive zone: 84px dock pill + 12px margin
    );

    auto dock = txui::make_ref<txui::DockWidget>();
    
    window->set_root_widget(dock);
    
    int ipc_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (ipc_fd >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::string sock_path = tinexus::common::RuntimePaths::get_ipc_socket_path();
        strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(ipc_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0 || errno == EINPROGRESS) {
            
            uint16_t sub_types[] = {
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_MINIMIZED),
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_RESTORED),
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED),
                static_cast<uint16_t>(tinexus::ipcd::protocol::DockMessageType::DOCK_QUERY_ICON_POSITION)
            };
            
            for (uint16_t t : sub_types) {
                struct {
                    tinexus::ipcd::protocol::Header hdr;
                    uint16_t topic;
                } __attribute__((packed)) msg;
                msg.hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
                msg.hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
                msg.hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC);
                msg.hdr.payload_len = sizeof(msg.topic);
                msg.hdr.sequence_id = 0;
                msg.hdr.flags = 0;
                msg.hdr.checksum = 0;
                msg.topic = t;
                send(ipc_fd, &msg, sizeof(msg), MSG_NOSIGNAL);
            }
        } else {
            close(ipc_fd);
            ipc_fd = -1;
        }
    }
    
    if (ipc_fd >= 0 && window->event_loop()) {
        window->event_loop()->add_fd(ipc_fd, [ipc_fd, dock_ptr = dock.get(), win_ptr = window.get()](int, uint32_t) {
            tinexus::ipcd::protocol::Header hdr;
            bool state_changed = false;
            while (true) {
                ssize_t peek_n = recv(ipc_fd, &hdr, sizeof(hdr), MSG_PEEK | MSG_DONTWAIT);
                if (peek_n <= 0) {
                    if (peek_n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                        if (win_ptr->event_loop()) {
                            win_ptr->event_loop()->remove_fd(ipc_fd);
                        }
                        close(ipc_fd);
                    }
                    break;
                }
                if (peek_n < static_cast<ssize_t>(sizeof(hdr))) {
                    break;
                }
                if (hdr.magic != tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC) {
                    char c;
                    if (recv(ipc_fd, &c, 1, 0) <= 0) break;
                    continue;
                }
                std::vector<uint8_t> buf(sizeof(hdr) + hdr.payload_len);
                ssize_t n = recv(ipc_fd, buf.data(), buf.size(), MSG_DONTWAIT);
                if (n == static_cast<ssize_t>(buf.size())) {
                    const void* payload = buf.data() + sizeof(hdr);
                    using namespace tinexus::ipcd::protocol;

                    if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_MINIMIZED)) {
                        auto* p = static_cast<const DockNotifyPayload*>(payload);
                        dock_ptr->update_icon_state(p->app_id, txui::DockIconAppState::Minimized);
                        state_changed = true;
                    } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_RESTORED)) {
                        auto* p = static_cast<const DockNotifyPayload*>(payload);
                        dock_ptr->update_icon_state(p->app_id, txui::DockIconAppState::RunningFocused);
                        state_changed = true;
                    } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_NOTIFY_FOCUS_CHANGED)) {
                        auto* p = static_cast<const DockFocusChangedPayload*>(payload);
                        if (p->is_focused) {
                            dock_ptr->update_icon_state(p->app_id, txui::DockIconAppState::RunningFocused);
                            for (auto& icon : dock_ptr->icons()) {
                                if (icon.app_id != p->app_id && icon.app_state == txui::DockIconAppState::RunningFocused) {
                                    dock_ptr->update_icon_state(icon.app_id, txui::DockIconAppState::RunningBg);
                                }
                            }
                        } else {
                            dock_ptr->update_icon_state(p->app_id, txui::DockIconAppState::RunningBg);
                        }
                        state_changed = true;
                    } else if (hdr.msg_type == static_cast<uint16_t>(DockMessageType::DOCK_QUERY_ICON_POSITION)) {
                        auto* p = static_cast<const DockQueryIconPositionPayload*>(payload);
                        int32_t x = 0, y = 0, w = 0, h = 0;
                        for (const auto& icon : dock_ptr->icons()) {
                            if (icon.app_id == p->app_id) {
                                double scale = icon.scale_spring.value + icon.bounce_offset_spring.value;
                                double size = 48.0 * scale;
                                w = static_cast<int32_t>(size);
                                h = static_cast<int32_t>(size);
                                x = static_cast<int32_t>(dock_ptr->frame().x() + icon.center_x - size/2.0);
                                y = static_cast<int32_t>(dock_ptr->frame().y() + (dock_ptr->frame().height() - size) / 2.0);
                                break;
                            }
                        }

                        struct {
                            tinexus::ipcd::protocol::Header reply_hdr;
                            DockIconPositionPayload reply_pld;
                        } __attribute__((packed)) reply;

                        reply.reply_hdr.magic = TINEXUS_IPC_MAGIC;
                        reply.reply_hdr.version = TINEXUS_IPC_VERSION_1;
                        reply.reply_hdr.msg_type = static_cast<uint16_t>(DockMessageType::DOCK_ICON_POSITION);
                        reply.reply_hdr.payload_len = sizeof(reply.reply_pld);
                        reply.reply_hdr.sequence_id = hdr.sequence_id;
                        reply.reply_hdr.flags = 0;
                        reply.reply_hdr.checksum = 0;

                        strncpy(reply.reply_pld.app_id, p->app_id, sizeof(reply.reply_pld.app_id)-1);
                        reply.reply_pld.x = x;
                        reply.reply_pld.y = static_cast<int32_t>(win_ptr->height()) - 72 + y;
                        reply.reply_pld.w = w;
                        reply.reply_pld.h = h;

                        send(ipc_fd, &reply, sizeof(reply), MSG_NOSIGNAL);
                    }
                } else {
                    break; // Incomplete message
                }
            }
            if (state_changed) {
                win_ptr->request_repaint();
            }
        });
    }

    bool needs_repaint = true;
    while (!window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                window->close();
            } else if (event.type == txui::EventType::PointerMove ||
                       event.type == txui::EventType::PointerLeave ||
                       event.type == txui::EventType::PointerButtonPress ||
                       event.type == txui::EventType::PointerButtonRelease) {
                if (dock && dock->handle_event(event)) {
                    needs_repaint = true;
                }
            }
        }
        
        bool is_animating = false;
        if (dock && dock->tick_animations(0.016)) {
            needs_repaint = true;
            is_animating = true;
        }
        
        if (needs_repaint || window->needs_repaint()) {
            window->present();
            needs_repaint = false;
        }
        
        if (is_animating) {
            window->wait_timeout(16); // 60 FPS while physics springs step
        } else {
            window->wait(); // Zero-CPU idle wait; wakes immediately on Wayland or IPC fd events
        }
    }
    
    if (ipc_fd >= 0) {
        if (window->event_loop()) {
            window->event_loop()->remove_fd(ipc_fd);
        }
        close(ipc_fd);
    }
    return 0;
}
