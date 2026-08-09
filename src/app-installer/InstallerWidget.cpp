#include "InstallerWidget.hpp"
#include <txui/render/Painter.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <txui/input/Event.hpp>
#include <ipcd/protocol/install.hpp>
#include <ipcd/protocol/header.hpp>
#include <common/logger.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <cctype>

namespace tinexus::app_installer {

// Convert txui::Key to a printable character
static char key_to_char(txui::Key key, bool shift_pressed) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
        if (shift_pressed) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9) {
        char c = static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
        if (shift_pressed && c == '0') return ')';
        if (shift_pressed && c == '9') return '(';
        return c;
    }
    if (key == txui::Key::Space) return ' ';
    if (key == txui::Key::Slash) return shift_pressed ? '?' : '/';
    if (key == txui::Key::Period) return shift_pressed ? '>' : '.';
    if (key == txui::Key::Minus) return shift_pressed ? '_' : '-';
    return '\0';
}

static bool send_install_request(int fd, const std::string& app_name, const std::array<uint8_t, 64>& sig) {
    int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::string path = "/run/user/" + std::to_string(getuid()) + "/tinexus/ipc.sock";
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    // Prepare payload
    tinexus::ipcd::protocol::Header hdr = {0};
    hdr.magic = tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC;
    hdr.version = tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1;
    hdr.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_INSTALL_REQUEST);
    
    tinexus::ipcd::protocol::InstallRequestPayload req;
    req.signature = sig;
    req.app_name_len = static_cast<uint16_t>(app_name.size());
    hdr.payload_len = sizeof(req) + req.app_name_len;

    // Send using sendmsg with SCM_RIGHTS
    struct msghdr msg = {0};
    struct iovec iov[3];
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = &req;
    iov[1].iov_len = sizeof(req);
    iov[2].iov_base = const_cast<char*>(app_name.c_str());
    iov[2].iov_len = app_name.size();
    
    msg.msg_iov = iov;
    msg.msg_iovlen = 3;

    char buf[CMSG_SPACE(sizeof(int))];
    memset(buf, 0, sizeof(buf));
    
    if (fd >= 0) {
        msg.msg_control = buf;
        msg.msg_controllen = sizeof(buf);
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
    }

    if (sendmsg(sock, &msg, 0) < 0) {
        close(sock);
        return false;
    }
    
    // Wait for response
    tinexus::ipcd::protocol::Header resp_hdr = {0};
    if (recv(sock, &resp_hdr, sizeof(resp_hdr), MSG_WAITALL) != sizeof(resp_hdr)) {
        close(sock);
        return false;
    }
    
    close(sock);
    return resp_hdr.msg_type == static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_INSTALL_OK);
}

InstallerWidget::InstallerWidget() = default;

bool InstallerWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        bool hover = (event.pointer.x >= m_btn_rect.x() && event.pointer.x <= m_btn_rect.right() &&
                      event.pointer.y >= m_btn_rect.y() && event.pointer.y <= m_btn_rect.bottom());
        if (hover != m_btn_hovered) {
            m_btn_hovered = hover;
            mark_needs_paint();
        }
        return true;
    }
    
    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            if (m_btn_hovered) {
                m_btn_pressed = true;
                mark_needs_paint();
                return true;
            }
        }
    }
    
    if (event.type == txui::EventType::PointerButtonRelease) {
        if (event.pointer.button == txui::MouseButton::Left) {
            if (m_btn_pressed) {
                m_btn_pressed = false;
                if (m_btn_hovered) {
                    do_install();
                }
                mark_needs_paint();
                return true;
            }
        }
    }
    
    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Backspace) {
            if (!m_path_input.empty()) {
                m_path_input.pop_back();
                mark_needs_paint();
            }
        } else if (event.keyboard.key == txui::Key::Enter) {
            do_install();
        } else {
            bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
            char c = key_to_char(event.keyboard.key, shift);
            if (c != '\0') {
                m_path_input += c;
                mark_needs_paint();
            }
        }
        return true;
    }
    
    return false;
}

txui::Size InstallerWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return txui::Size(constraints.max_width, constraints.max_height);
}

void InstallerWidget::layout_override(const txui::Rect& frame) noexcept {
    double cx = frame.width() / 2.0;
    m_input_rect = txui::Rect(cx - 250, 160, 500, 40);
    m_btn_rect = txui::Rect(cx - 90, 240, 180, 40);
}

void InstallerWidget::paint_override(txui::Painter& painter) const noexcept {
    // Background
    painter.fill_gradient_rect(frame(), txui::Color(10, 10, 18, 255), txui::Color(16, 16, 26, 255));
    
    double cx = frame().width() / 2.0;
    double y = 100.0;
    
    painter.draw_text(txui::Point(cx - 150, y), "Install Tinexus App (.txapp)", txui::Color(240, 240, 255, 255), 2.0);
    y += 60;
    
    // Input box
    painter.fill_rounded_rect(m_input_rect, 8.0, txui::Color(24, 24, 36, 255));
    
    std::string disp_path = m_path_input;
    if (disp_path.empty()) {
        painter.draw_text(txui::Point(m_input_rect.x() + 10, m_input_rect.y() + 12), "Enter path to .txapp file...", txui::Color(120, 120, 140, 255), 1.0);
    } else {
        painter.draw_text(txui::Point(m_input_rect.x() + 10, m_input_rect.y() + 12), disp_path, txui::Color(240, 240, 255, 255), 1.0);
    }
    
    // Button
    painter.fill_gradient_rounded_rect(m_btn_rect, 8.0, 
        m_btn_hovered ? txui::Color(120, 150, 250, 255) : txui::Color(107, 140, 239, 255),
        m_btn_hovered ? txui::Color(90, 120, 220, 255) : txui::Color(80, 110, 220, 255));
    painter.draw_text(txui::Point(m_btn_rect.x() + 45, m_btn_rect.y() + 12), "Install App", txui::Color(255, 255, 255, 255), 1.0);
    
    y += 140;
    
    if (!m_status_msg.empty()) {
        txui::Color status_col = m_status_is_error ? txui::Color(250, 100, 100, 255) : txui::Color(100, 250, 100, 255);
        painter.draw_text(txui::Point(cx - 150, y), m_status_msg, status_col, 1.0);
    }
}

void InstallerWidget::do_install() {
    if (m_path_input.empty()) {
        m_status_msg = "Please enter a valid path.";
        m_status_is_error = true;
        mark_needs_paint();
        return;
    }
    
    int fd = open(m_path_input.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        m_status_msg = "Error: Could not open file (check path).";
        m_status_is_error = true;
        mark_needs_paint();
        return;
    }
    
    // Read signature
    std::string sig_path = m_path_input + ".sig";
    std::array<uint8_t, 64> sig_bytes = {0};
    std::ifstream sig_file(sig_path, std::ios::binary);
    if (sig_file.is_open()) {
        sig_file.read(reinterpret_cast<char*>(sig_bytes.data()), 64);
    }
    
    // Extract app name from path (e.g. "/path/to/tinexus-notes.txapp" -> "tinexus-notes")
    size_t slash = m_path_input.find_last_of('/');
    std::string basename = (slash == std::string::npos) ? m_path_input : m_path_input.substr(slash + 1);
    size_t ext = basename.find(".txapp");
    std::string app_name = (ext == std::string::npos) ? basename : basename.substr(0, ext);
    
    bool ok = send_install_request(fd, app_name, sig_bytes);
    close(fd);
    
    if (ok) {
        m_status_msg = "Successfully installed " + app_name + "!";
        m_status_is_error = false;
        m_path_input.clear();
    } else {
        m_status_msg = "Error: Installation failed (Gatekeeper blocked or invalid).";
        m_status_is_error = true;
    }
    mark_needs_paint();
}

} // namespace tinexus::app_installer
