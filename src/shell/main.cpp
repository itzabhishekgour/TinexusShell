// tinexus-shell — Main Shell Entry Point
#include "DesktopShellWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/input/Event.hpp>
#include <ipcd/protocol/header.hpp>
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

    uint16_t topic = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::LAUNCHER_OPEN);
    tinexus::ipcd::protocol::Header sh{};
    sh.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    sh.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    sh.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC);
    sh.payload_len = sizeof(topic);
    send(fd, &sh, sizeof(sh), MSG_NOSIGNAL);
    send(fd, &topic, sizeof(topic), MSG_NOSIGNAL);
    log::info("[Shell] Connected to ipcd and subscribed to LAUNCHER_OPEN topic");

    while (true) {
        tinexus::ipcd::protocol::Header rx{};
        ssize_t got = 0;
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
    }
done:
    close(fd);
    g_ipc_fd.store(-1);
    log::warn("[Shell] ipcd connection lost");
}

static void trigger_launcher() {
    int fd = g_ipc_fd.load();
    if (fd >= 0) {
        tinexus::ipcd::protocol::Header sh{};
        sh.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        sh.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        sh.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::LAUNCHER_OPEN);
        sh.payload_len = 0;
        send(fd, &sh, sizeof(sh), MSG_NOSIGNAL);
        log::info("[Shell] Sent LAUNCHER_OPEN notification to ipcd");
    } else {
        log::info("[Shell] Spawning tinexus-launcher process directly");
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            execlp("tinexus-launcher", "tinexus-launcher", nullptr);
            _exit(127);
        }
    }
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

    bool needs_redraw = false;

    shell_widget->on_resize_requested = [&](double new_h) {
        window->resize(1920, static_cast<uint32_t>(new_h));
        window->set_keyboard_interactivity(new_h > 46.0);
        needs_redraw = true;
    };

    shell_widget->on_app_launch = [&](const AppItem& item) {
        spawn_app(item);
        shell_widget->close_all_flyouts();
        needs_redraw = true;
    };

    shell_widget->on_pulse_toggle_requested = [&]() {
        trigger_launcher();
    };

    window->set_root_widget(txui::Ref<txui::Widget>(shell_widget.get()));
    shell_widget->mark_needs_paint();
    window->present();

    bool running = true;

    while (running && !window->should_close()) {
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
                const bool ctrl = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);

                if (ctrl && (event.keyboard.key == txui::Key::K || event.keyboard.key == txui::Key::Space)) {
                    trigger_launcher();
                    continue;
                }

                if (event.keyboard.key == txui::Key::Escape) {
                    shell_widget->close_all_flyouts();
                } else {
                    shell_widget->handle_event(event);
                }
            }
        }

        if (!running) break;

        if (needs_redraw) {
            window->present();
            needs_redraw = false;
        }

        window->wait_timeout(16);
    }

    log::info("[Shell] Exiting cleanly.");
    return 0;
}
