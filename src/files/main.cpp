#include "files/column_view_model.hpp"
#include "files/trash_manager.hpp"
#include "files/file_operations.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include "ipcd/protocol/header.hpp"
#include "ipcd/protocol/uninstall.hpp"
#include <iostream>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <txui/window/Window.hpp>
#include <txui/input/Event.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include "ui/ColumnBrowserWidget.hpp"


int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("files");
    tinexus::log::info("Starting Tinexus Files...");
    signal(SIGCHLD, SIG_IGN);

    // Initialize SDK client for platform services
    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Files: Connected to Tinexus Platform IPC broker.");
    }

    std::filesystem::path target_path;
    if (argc > 1) {
        target_path = std::filesystem::path(argv[1]);
    } else {
        if (std::filesystem::exists("/usr/share/backgrounds")) {
            target_path = "/usr/share/backgrounds";
        } else {
            const char* home = std::getenv("HOME");
            target_path = (home && *home && std::filesystem::exists(home)) ? std::filesystem::path(home) : std::filesystem::current_path();
        }
    }

    auto window = txui::Window::create(1000, 620, "Tinexus Files", false, "tinexus-files");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("Failed to connect to Wayland display.");
        return 1;
    }

    auto files_window = txui::make_ref<tinexus::files::ui::ColumnBrowserWidget>();
    // Wrap in ChromeWidget — provides macOS-style window chrome with
    // fully functional close, minimize, and maximize buttons.
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Tinexus Files",
        files_window,
        // ── Close: request application exit ──────────────────────────────────
        [w = window.get()]() {
            w->on_close_request();
        },
        // ── Minimize: hide window via xdg_toplevel.minimize request ──────────
        // Compositor will hide the scene node. No restore until dock exists.
        [w = window.get()]() {
            w->minimize();
        },
        // ── Maximize: toggle maximize/restore ─────────────────────────────────
        [w = window.get()]() {
            w->set_maximized(!w->is_maximized());
        },
        // ── Move: initiate interactive drag via xdg_toplevel.move ─────────────
        [w = window.get()](uint32_t serial) {
            w->start_interactive_move(serial);
        }
    );
    files_window->set_on_execute([&](const std::filesystem::path& path) {
        if (sdk_client.is_connected()) {
            tinexus::log::info("Launching {} via SDK", path.string());
            tinexus::ActionRequest req;
            req.type = tinexus::ActionType::OpenFile;
            req.target = path.string();
            
            // Pass FD to prevent TOCTOU attacks
            int fd = open(path.string().c_str(), O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                req.target_fd = fd;
            }

            auto res = sdk_client.actions().execute(req);
            
            if (fd >= 0) close(fd); // Cleanup after IPC dispatch

            if (!res.is_ok()) {
                tinexus::log::error("Failed to launch file: {}", res.error().message);
            }
        }
    });

    
    files_window->set_on_uninstall([&](const std::filesystem::path& path) {
        std::string app_name = path.stem().string(); // "/opt/tinexus-apps/app.txapp" -> "app"
        tinexus::log::info("Requesting uninstall of app '{}'", app_name);

        int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            tinexus::log::error("Failed to create socket for uninstall request");
            return;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::string sock_path = "/run/user/" + std::to_string(getuid()) + "/tinexus/ipc.sock";
        strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            tinexus::log::error("Failed to connect to IPC broker for uninstall");
            close(sock);
            return;
        }

        // Prepare payload exactly like InstallerWidget
        tinexus::ipcd::protocol::Header hdr = {};
        hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
        hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
        hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_UNINSTALL_REQUEST);
        
        tinexus::ipcd::protocol::UninstallRequestPayload req = {}; // Ensure null initialization
        strncpy(req.app_name, app_name.c_str(), sizeof(req.app_name) - 1);
        req.app_name[sizeof(req.app_name) - 1] = '\0'; // Explicit null-termination

        hdr.payload_len = sizeof(req);

        // Send using sendmsg
        struct msghdr msg = {};
        struct iovec iov[2];
        iov[0].iov_base = &hdr;
        iov[0].iov_len = sizeof(hdr);
        iov[1].iov_base = &req;
        iov[1].iov_len = sizeof(req);
        
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        if (sendmsg(sock, &msg, 0) < 0) {
            tinexus::log::error("Failed to send SYS_UNINSTALL_REQUEST");
            close(sock);
            return;
        }
        
        // Wait for response
        tinexus::ipcd::protocol::Header resp_hdr = {};
        if (recv(sock, &resp_hdr, sizeof(resp_hdr), 0) == sizeof(resp_hdr)) {
            if (resp_hdr.msg_type == static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_UNINSTALL_OK)) {
                tinexus::log::info("Uninstall successful");
            } else {
                tinexus::log::error("Uninstall failed");
            }
        }
        close(sock);
    });

    files_window->set_on_trash([&](const std::filesystem::path& path) {
        tinexus::log::info("Moving {} to trash", path.string());
        tinexus::files::TrashManager::instance().move_to_trash(path);
        // Note: We need to trigger a refresh on the parent directory, but for MVP it's just logging.
    });

    window->set_root_widget(chrome);
    files_window->navigate_to(target_path);

    window->present();

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        bool needs_redraw = false;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else {
                if (chrome->handle_event(event)) {
                    needs_redraw = true;
                }
            }
        }
        
        if (chrome->needs_paint() || chrome->needs_layout()) {
            needs_redraw = true;
        }

        if (needs_redraw) {
            window->present();
        }

        window->wait_timeout(16); // 60fps max polling
    }

    sdk_client.disconnect();
    tinexus::log::info("Tinexus Files exiting cleanly.");
    return 0;
}
