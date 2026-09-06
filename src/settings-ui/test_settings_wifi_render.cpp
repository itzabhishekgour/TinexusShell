#include "settings/SettingsWidget.hpp"
#include "settings/WifiManager.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <unistd.h>

using namespace tinexus;
using namespace tinexus::settings_ui;

int main() {
    std::cout << "[Visual Test] Initializing SettingsWidget..." << std::endl;
    auto widget = txui::make_ref<SettingsWidget>();

    txui::Constraints constraints(0, 1000, 0, 640);
    widget->measure(constraints);
    widget->layout(txui::Rect(0, 0, 1000, 640));

    // 1. Switch to Network Tab (Key::N3)
    txui::Event e3;
    e3.type = txui::EventType::KeyDown;
    e3.keyboard.key = txui::Key::N3;
    e3.keyboard.modifiers = txui::KeyModifier::None;
    widget->handle_event(e3);

    // Wait up to 3s for background scan thread to finish populating networks
    for (int i = 0; i < 30; ++i) {
        if (!WifiManager::instance().is_scanning()) break;
        usleep(100000);
    }

    txui::Canvas canvas(1000, 640);
    canvas.clear(txui::Color(18, 18, 24, 255));
    txui::PixmanBackend backend;

    // Render Network Page (Card list populated)
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);
    }

    if (!txui::ImageWriter::save_png(canvas, "wifi_settings_network_page.png")) {
        std::cerr << "FAIL: Failed to save wifi_settings_network_page.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Successfully saved wifi_settings_network_page.png" << std::endl;

    // 2. Summon modal explicitly for test rendering
    widget->open_wifi_password_modal("OPPO F23 5G");

    // Type a password into modal: "SecretPass123"
    std::string test_pw = "SecretPass123";
    for (char c : test_pw) {
        txui::Event key_ev;
        key_ev.type = txui::EventType::KeyDown;
        key_ev.keyboard.modifiers = txui::KeyModifier::None;
        if (c >= 'a' && c <= 'z') {
            key_ev.keyboard.key = static_cast<txui::Key>(static_cast<int>(txui::Key::A) + (c - 'a'));
        } else if (c >= 'A' && c <= 'Z') {
            key_ev.keyboard.key = static_cast<txui::Key>(static_cast<int>(txui::Key::A) + (c - 'A'));
            key_ev.keyboard.modifiers = txui::KeyModifier::Shift;
        } else if (c >= '0' && c <= '9') {
            key_ev.keyboard.key = static_cast<txui::Key>(static_cast<int>(txui::Key::N0) + (c - '0'));
        }
        widget->handle_event(key_ev);
    }

    // Render Modal Dialog Overlay
    txui::Canvas modal_canvas(1000, 640);
    modal_canvas.clear(txui::Color(18, 18, 24, 255));
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, modal_canvas);
    }

    if (!txui::ImageWriter::save_png(modal_canvas, "wifi_settings_modal_dialog.png")) {
        std::cerr << "FAIL: Failed to save wifi_settings_modal_dialog.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Successfully saved wifi_settings_modal_dialog.png" << std::endl;

    return 0;
}
