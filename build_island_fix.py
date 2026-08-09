import os

with open('launcher_main.cpp', 'r') as f:
    orig = f.read()

ipc_code = """
#include "Theme.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <ctime>
#include <algorithm>

static std::atomic<bool> g_toggle_search{false};

void ipc_listener_thread() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/user/%d/tinexus/ipc.sock", getuid());
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(sock);
        return;
    }
    struct __attribute__((packed)) {
        uint32_t magic = 0x544E5853;
        uint16_t version = 0x0100;
        uint16_t msg_type = 2;
        uint16_t flags = 0;
        uint32_t sequence_id = 0;
        uint32_t payload_len = 2;
        uint32_t checksum = 0;
        uint16_t topic = 1000;
    } sub_req;
    send(sock, &sub_req, sizeof(sub_req), 0);
    
    while (true) {
        struct __attribute__((packed)) {
            uint32_t magic;
            uint16_t version;
            uint16_t msg_type;
            uint16_t flags;
            uint32_t sequence_id;
            uint32_t payload_len;
            uint32_t checksum;
        } hdr;
        if (recv(sock, &hdr, sizeof(hdr), 0) <= 0) break;
        if (hdr.msg_type == 1004) { g_toggle_search = true; }
        if (hdr.payload_len > 0) {
            char dump[4096];
            int to_read = hdr.payload_len;
            while (to_read > 0) {
                int r = recv(sock, dump, std::min(to_read, 4096), 0);
                if (r <= 0) break;
                to_read -= r;
            }
        }
    }
    close(sock);
}

"""
orig = orig.replace('using namespace tinexus;', 'using namespace tinexus;\n' + ipc_code)

island_widget = """
class IslandWidget : public LauncherWidget {
public:
    bool is_search_mode = false;
    
    void paint_override(txui::Painter& painter) const override {
        painter.fill_rect(bounds(), Theme::SURFACE_ELEVATED, 24);
        if (is_search_mode) {
            LauncherWidget::paint_override(painter);
        } else {
            time_t now = time(nullptr);
            struct tm tm_buf;
#if defined(_WIN32)
            localtime_s(&tm_buf, &now);
#else
            localtime_r(&now, &tm_buf);
#endif
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%H:%M", &tm_buf);
            painter.draw_text({bounds().x + 24, bounds().y + bounds().height/2 + 8}, 
                              time_str, Theme::TXT_PRIMARY, 16);
            painter.fill_rect({bounds().x + bounds().width/2 - 16, bounds().y + 8, 32, 32}, Theme::ACCENT_COLOR, 16);
            painter.draw_text({bounds().x + bounds().width - 64, bounds().y + bounds().height/2 + 8}, 
                              "100%", Theme::TXT_PRIMARY, 16);
        }
    }
};
"""

# Insert AFTER LauncherWidget ends (right before main)
main_start = orig.find('int main(int argc, char** argv)')
orig = orig[:main_start] + island_widget + orig[main_start:]

main_code = orig[main_start:]

# Fix Window create and add thread start
main_code = main_code.replace('auto window = txui::Window::create(900, 700, "Tinexus Launcher");', 
                              'auto window = txui::Window::create(360, 48, "Tinexus Launcher", true);\n    std::thread(ipc_listener_thread).detach();')

main_code = main_code.replace('auto launcher_widget = txui::make_ref<LauncherWidget>();',
                              'auto launcher_widget = txui::make_ref<IslandWidget>();')

main_code = main_code.replace('while (running && !window->should_close()) {',
                              'while (running && !window->should_close()) {\n        if (g_toggle_search.exchange(false)) {\n            launcher_widget->is_search_mode = !launcher_widget->is_search_mode;\n            if (launcher_widget->is_search_mode) {\n                window->resize(800, 600);\n            } else {\n                window->resize(360, 48);\n            }\n            launcher_widget->mark_needs_paint();\n        }')

# Also fix the cast in set_root_widget to avoid Ref type errors since IslandWidget subclasses LauncherWidget
main_code = main_code.replace('window->set_root_widget(launcher_widget);', 'window->set_root_widget(txui::Ref<txui::Widget>(launcher_widget.get()));')

with open('src/shell/main.cpp', 'w') as f:
    f.write(orig[:main_start] + main_code)
