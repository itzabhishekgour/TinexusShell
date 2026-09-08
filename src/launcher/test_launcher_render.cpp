#include <launcher/LauncherWidget.hpp>
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <vector>

using namespace txui;
using namespace tinexus::launcher;

static std::vector<AppItem> get_mock_apps() {
    return {
        {"Tinexus Terminal", "tinexus-terminal", "Default Wayland GPU Terminal", true, "utilities-terminal", ResultKind::App},
        {"Tinexus Files", "tinexus-files", "Lightweight Miller Column File Manager", false, "system-file-manager", ResultKind::App},
        {"Tinexus Settings", "tinexus-settings-ui", "System Configuration & Control Center", false, "preferences-desktop", ResultKind::App},
        {"Activity Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", false, "utilities-system-monitor", ResultKind::App},
        {"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", false, "system-software-install", ResultKind::App},
    };
}

static void render_launcher_frame(const std::string& name,
                                  const std::function<void(LauncherWidget&)>& setup,
                                  const std::string& filename) {
    std::cout << "[Visual Test] Rendering Launcher: " << name << " -> " << filename << std::endl;
    const uint32_t W = 900, H = 700;
    Canvas canvas(W, H);
    canvas.clear(Color(8, 9, 14, 255));
    PixmanBackend backend;

    auto launcher = make_ref<LauncherWidget>();
    launcher->set_all_apps(get_mock_apps());
    setup(*launcher);

    Constraints constraints(0, W, 0, H);
    launcher->measure(constraints);
    launcher->layout(Rect(0, 0, W, H));

    CommandBuffer cmds;
    Painter painter(cmds);
    painter.begin_frame();
    launcher->paint(painter);
    painter.end_frame();
    backend.execute(cmds, canvas);

    if (ImageWriter::save_png(canvas, filename)) {
        std::cout << "  [SUCCESS] Saved " << filename << std::endl;
    } else {
        std::cerr << "  [FAILURE] Could not save " << filename << std::endl;
    }
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " TxUI Launcher Visual Verification Harness        " << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Empty State (placeholder search input, recent/suggested applications)
    render_launcher_frame("Launcher Empty State", [](LauncherWidget& lw) {
        lw.set_query("");
        lw.set_selected_index(0);
    }, "launcher_empty_state.png");

    // 2. Search Results with Keyboard Navigation (search query 'sett', navigated down to Settings)
    render_launcher_frame("Launcher Search Results", [](LauncherWidget& lw) {
        // Simulate typing into TextInput via handle_event or set_query
        lw.set_query("sett");
        // Verify Up/Down event routing: Down key navigates selection
        Event ev_down;
        ev_down.type = EventType::KeyDown;
        ev_down.keyboard.key = Key::Down;
        ev_down.keyboard.modifiers = KeyModifier::None;
        lw.handle_event(ev_down);
    }, "launcher_search_results.png");

    // 3. Calculator State (evaluating '42 * 8')
    render_launcher_frame("Launcher Calculator", [](LauncherWidget& lw) {
        lw.set_query("42 * 8");
        lw.set_selected_index(0);
    }, "launcher_calculator.png");

    return 0;
}
