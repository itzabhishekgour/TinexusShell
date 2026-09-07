// tinexus-shell — Main Shell Entry Point
#include "DesktopShellWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/input/Event.hpp>
#include <common/logger.hpp>
#include <csignal>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <atomic>

using namespace tinexus;
using namespace tinexus::shell;

static std::atomic<int> g_ipc_fd{-1};
static std::atomic<bool> g_toggle_pulse{false};

#pragma pack(push, 1)
struct IpcHdr {
    uint32_t magic = 0x544E5853; uint16_t version = 0x0100; uint16_t msg_type;
    uint16_t flags = 0; uint32_t seq = 0; uint32_t payload_len; uint32_t csum = 0;
};
#pragma pack(pop)

static char key_to_char(txui::Key key, bool shift) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
        if (shift) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9)
        return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
    if (key == txui::Key::Space) return ' ';
    return '\0';
}

void ipc_listener_thread() {
    int fd = -1;
    for (int a = 0; a < 30 && fd < 0; ++a) {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) { ::sleep(1); continue; }
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        char path[108];
        snprintf(path, sizeof(path), "/run/user/%d/tinexus/ipc.sock", static_cast<int>(getuid()));
        memcpy(addr.sun_path, path, strlen(path) + 1);
        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(fd); fd = -1; ::sleep(1);
        }
    }
    if (fd < 0) { log::warn("[Shell] Could not connect to ipcd"); return; }
    g_ipc_fd.store(fd);

    uint16_t topic = 1000;
    IpcHdr sh; sh.msg_type = 2 /* SYS_SUBSCRIBE_TOPIC */; sh.payload_len = sizeof(topic);
    send(fd, &sh, sizeof(sh), MSG_NOSIGNAL);
    send(fd, &topic, sizeof(topic), MSG_NOSIGNAL);
    log::info("[Shell] Subscribed to LAUNCHER_OPEN (1000) via ipcd");

    while (true) {
        IpcHdr rx; ssize_t got = 0;
        auto* raw = reinterpret_cast<uint8_t*>(&rx);
        while (got < static_cast<ssize_t>(sizeof(rx))) {
            ssize_t n = recv(fd, raw + got, sizeof(rx) - static_cast<size_t>(got), 0);
            if (n <= 0) goto done;
            got += n;
        }
        std::vector<uint8_t> buf;
        if (rx.payload_len > 0) {
            buf.resize(rx.payload_len); ssize_t pg = 0;
            while (pg < static_cast<ssize_t>(rx.payload_len)) {
                ssize_t n = recv(fd, buf.data() + pg, rx.payload_len - static_cast<size_t>(pg), 0);
                if (n <= 0) goto done;
                pg += n;
            }
        }
        if (rx.msg_type == 1004) {
            g_toggle_pulse.store(true);
            log::info("[Shell] LAUNCHER_SHOW received — toggling Pulse");
        }
    }
done:
    close(fd);
    log::warn("[Shell] ipcd connection lost");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    log::set_component_name("shell");
    log::info("[Shell] Tinexus Unified Desktop Shell starting...");
    signal(SIGCHLD, SIG_IGN);

    auto window = txui::Window::create(1920, 46, "Aura", /*layer_shell=*/true);
    std::thread(ipc_listener_thread).detach();

    if (!window || !window->is_wayland_connected()) {
        log::error("[Shell] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    window->set_layer_shell_config(txui::LayerType::Top,
        txui::LayerAnchor::Top | txui::LayerAnchor::Left | txui::LayerAnchor::Right, 32);

    auto shell_widget = txui::make_ref<DesktopShellWidget>();

    double current_w = 1920.0;
    double current_h = 46.0;
    double target_w  = 1920.0;
    double target_h  = 46.0;
    bool animating   = false;
    bool launch_animating = false;
    bool launch_flipped   = false;
    bool needs_redraw     = false;

    shell_widget->on_resize_requested = [&](double new_h) {
        if (!shell_widget->pulse_active && !launch_animating) {
            target_h = new_h;
            current_h = new_h;
            window->resize(1920, static_cast<uint32_t>(new_h));
            window->set_keyboard_interactivity(new_h > 46.0);
            needs_redraw = true;
        }
    };

    shell_widget->on_app_launch = [&](const AppItem& item) {
        shell_widget->pulse_launch_app = item;
        shell_widget->active_app_name = item.name;
        launch_animating = true;
        launch_flipped = false;
        animating = true;
        target_w = 1920.0;
        target_h = 46.0;
        needs_redraw = true;
    };

    window->set_root_widget(txui::Ref<txui::Widget>(shell_widget.get()));
    shell_widget->mark_needs_paint();
    window->present();

    bool running = true;

    while (running && !window->should_close()) {
        if (g_toggle_pulse.exchange(false)) {
            shell_widget->pulse_active = !shell_widget->pulse_active;
            shell_widget->close_all_flyouts();

            if (shell_widget->pulse_active) {
                log::info("[Shell] Expanding to Pulse mode (Ctrl+K animated)");
                shell_widget->pulse_query = "";
                shell_widget->pulse_selected_index = 0;
                shell_widget->pulse_results = build_results("", shell_widget->all_apps);
                target_w = 1920.0;
                target_h = 580.0;
                window->set_keyboard_interactivity(true);
            } else {
                log::info("[Shell] Collapsing to Top Bar Aura mode");
                target_w = 1920.0;
                target_h = 46.0;
                window->set_keyboard_interactivity(false);
            }
            animating = true;
            shell_widget->mark_needs_paint();
            needs_redraw = true;
        }

        // ── Animation tick (Ease-out 60 FPS) ──────────────────────────────
        if (animating) {
            double dw = target_w - current_w;
            double dh = target_h - current_h;

            current_w += dw * 0.4;
            current_h += dh * 0.4;
            if (std::abs(dw) < 1.0 && std::abs(dh) < 1.0) {
                current_w = target_w;
                current_h = target_h;
                if (!launch_animating) animating = false;
            }
            if (launch_animating) {
                if (!launch_flipped && current_h <= 60.0) {
                    launch_flipped = true;
                    shell_widget->pulse_launching = true;
                    target_w = 1920.0;
                    target_h = 340.0;
                } else if (launch_flipped && std::abs(dh) < 2.0) {
                    spawn_app(shell_widget->pulse_launch_app);
                    launch_animating = false;
                    launch_flipped = false;
                    shell_widget->pulse_launching = false;
                    shell_widget->pulse_active = false;
                    shell_widget->pulse_query = "";
                    shell_widget->pulse_selected_index = 0;
                    target_w = 1920.0;
                    target_h = 46.0;
                    window->set_keyboard_interactivity(false);
                    animating = true;
                }
            }
            if (current_w > 0.0 && current_h > 0.0) {
                window->resize(static_cast<uint32_t>(current_w), static_cast<uint32_t>(current_h));
                needs_redraw = true;
            }
        }

        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::PointerMove ||
                       event.type == txui::EventType::PointerButtonPress ||
                       event.type == txui::EventType::PointerButtonRelease) {
                shell_widget->handle_event(event);
                needs_redraw = true;
            } else if (event.type == txui::EventType::KeyDown) {
                needs_redraw = true;
                const bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);

                if (shell_widget->pulse_active) {
                    if (event.keyboard.key == txui::Key::Escape) {
                        shell_widget->pulse_active = false;
                        shell_widget->pulse_query = "";
                        target_w = 1920.0;
                        target_h = 46.0;
                        window->set_keyboard_interactivity(false);
                        animating = true;
                        shell_widget->mark_needs_paint();
                        needs_redraw = true;
                    } else if (event.keyboard.key == txui::Key::Enter) {
                        if (!shell_widget->pulse_results.empty() &&
                            shell_widget->pulse_selected_index < shell_widget->pulse_results.size()) {
                            auto it = shell_widget->pulse_results[shell_widget->pulse_selected_index];
                            if (shell_widget->on_app_launch) {
                                shell_widget->on_app_launch(it);
                            }
                        }
                    } else if (event.keyboard.key == txui::Key::Up) {
                        if (shell_widget->pulse_selected_index > 0) {
                            shell_widget->pulse_selected_index--;
                            shell_widget->mark_needs_paint();
                        }
                    } else if (event.keyboard.key == txui::Key::Down) {
                        if (shell_widget->pulse_selected_index + 1 < shell_widget->pulse_results.size()) {
                            shell_widget->pulse_selected_index++;
                            shell_widget->mark_needs_paint();
                        }
                    } else if (event.keyboard.key == txui::Key::Backspace) {
                        if (!shell_widget->pulse_query.empty()) {
                            shell_widget->pulse_query.pop_back();
                            shell_widget->pulse_selected_index = 0;
                            shell_widget->pulse_results = build_results(shell_widget->pulse_query, shell_widget->all_apps);
                            shell_widget->mark_needs_paint();
                        }
                    } else {
                        char ch = key_to_char(event.keyboard.key, shift);
                        if (ch != '\0') {
                            shell_widget->pulse_query += ch;
                            shell_widget->pulse_selected_index = 0;
                            shell_widget->pulse_results = build_results(shell_widget->pulse_query, shell_widget->all_apps);
                            shell_widget->mark_needs_paint();
                        }
                    }
                } else if (event.keyboard.key == txui::Key::Escape) {
                    shell_widget->close_all_flyouts();
                }
            }
        }

        if (!running) break;

        if (needs_redraw) {
            window->present();
            needs_redraw = false;
        }

        window->wait_timeout(animating ? 16 : 100);
    }

    log::info("[Shell] Exiting cleanly.");
    return 0;
}
