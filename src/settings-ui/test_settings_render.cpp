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

static bool render_page_to_png(txui::Ref<SettingsWidget>& widget, const std::string& filename) {
    txui::Canvas canvas(1000, 640);
    canvas.clear(txui::Color(14, 14, 20, 255));
    txui::PixmanBackend backend;

    txui::CommandBuffer cmds;
    txui::Painter painter(cmds);
    painter.begin_frame();
    widget->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);

    if (!txui::ImageWriter::save_png(canvas, filename)) {
        std::cerr << "FAIL: Could not save " << filename << std::endl;
        return false;
    }
    std::cout << "[Visual Test] Successfully saved " << filename << std::endl;
    return true;
}

int main() {
    std::cout << "=== Tinexus Settings UI Visual Test Suite ===" << std::endl;

    auto widget = txui::make_ref<SettingsWidget>();
    txui::Constraints constraints(0, 1000, 0, 640);
    widget->measure(constraints);
    widget->layout(txui::Rect(0, 0, 1000, 640));

    // 1. Display Page
    widget->select_page(SettingsPage::Display);
    if (!render_page_to_png(widget, "settings_display_page.png")) return 1;

    // 1b. Sound Page
    widget->select_page(SettingsPage::Sound);
    if (!render_page_to_png(widget, "settings_sound_page.png")) return 1;

    // 2. Personalization Page
    widget->select_page(SettingsPage::Personalization);
    if (!render_page_to_png(widget, "settings_personalization_page.png")) return 1;

    // 3. Network / Wi-Fi Page
    widget->select_page(SettingsPage::Network);
    // Wait briefly for mock/live network scan
    for (int i = 0; i < 15; ++i) {
        if (!WifiManager::instance().is_scanning()) break;
        usleep(50000);
    }
    if (!render_page_to_png(widget, "settings_network_wifi_page.png")) return 1;

    // 4. Wi-Fi Password Modal Dialog
    widget->open_wifi_password_modal("Tinexus-5G-Ultra");
    // Type sample text into password input
    widget->select_page(SettingsPage::Network); // layout modal
    if (!render_page_to_png(widget, "settings_wifi_modal_open.png")) return 1;

    // Dismiss modal for subsequent pages
    txui::Event esc_ev;
    esc_ev.type = txui::EventType::KeyDown;
    esc_ev.keyboard.key = txui::Key::Escape;
    esc_ev.keyboard.modifiers = txui::KeyModifier::None;
    widget->handle_event(esc_ev);

    // 5. System & Power Page
    widget->select_page(SettingsPage::System);
    if (!render_page_to_png(widget, "settings_system_page.png")) return 1;

    // 6. Keyboard Shortcuts Page
    widget->select_page(SettingsPage::KeyboardShortcuts);
    if (!render_page_to_png(widget, "settings_shortcuts_page.png")) return 1;

    // 7. Privacy & Security Page
    widget->select_page(SettingsPage::PrivacySecurity);
    if (!render_page_to_png(widget, "settings_privacy_page.png")) return 1;

    // 8. About Page
    widget->select_page(SettingsPage::About);
    if (!render_page_to_png(widget, "settings_about_page.png")) return 1;

    std::cout << "=== All 8 Settings UI visual tests completed successfully! ===" << std::endl;
    return 0;
}
