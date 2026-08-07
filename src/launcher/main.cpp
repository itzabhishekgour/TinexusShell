#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/widgets/TextWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <common/logger.hpp>
#include <indexer/desktop_entry.hpp>
#include <unistd.h>   // fork, execvp, execlp, setsid
#include <csignal>    // signal, SIGCHLD, SIG_IGN
#include <cstdlib>    // _exit
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

using namespace tinexus;
namespace fs = std::filesystem;

struct AppItem {
    std::string name;
    std::string exec;
    std::string description;
    bool is_terminal{false};
    std::string icon;
};

// ---------------------------------------------------------------------------
// spawn_app — executes target binary or desktop entry command
// ---------------------------------------------------------------------------
static pid_t spawn_app(const std::string& raw_exec, bool is_terminal = false) {
    std::string clean_exec = indexer::DesktopParser::sanitize_exec(raw_exec);
    if (clean_exec.empty()) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        log::error("[Launcher] fork() failed for app '{}'", clean_exec);
        return -1;
    }
    if (pid == 0) {
        setsid();
        if (is_terminal) {
            execlp("foot", "foot", "-e", clean_exec.c_str(), nullptr);
            execlp("weston-terminal", "weston-terminal", nullptr);
            _exit(127);
        } else {
            std::vector<std::string> tokens;
            std::istringstream iss(clean_exec);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            if (tokens.empty()) _exit(1);

            std::vector<char*> args;
            for (auto& t : tokens) {
                args.push_back(const_cast<char*>(t.c_str()));
            }
            args.push_back(nullptr);

            execvp(args[0], args.data());
            _exit(127);
        }
    }
    log::info("[Launcher] Spawned app '{}' PID={}", clean_exec, pid);
    return pid;
}

// Convert txui::Key to char
static char key_to_char(txui::Key key, bool shift_pressed) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
        if (shift_pressed) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9) {
        return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
    }
    if (key == txui::Key::Space) return ' ';
    return '\0';
}

// Helper to convert string to lowercase
static std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

// ---------------------------------------------------------------------------
// Load applications from system .desktop directories + fallback apps
// ---------------------------------------------------------------------------
static std::string icon_to_emoji(const std::string& icon_name, const std::string& app_name) {
    std::string name = to_lower(icon_name);
    std::string app = to_lower(app_name);

    if (name.find("terminal") != std::string::npos || app.find("terminal") != std::string::npos || app.find("foot") != std::string::npos || app.find("alacritty") != std::string::npos)
        return "💻";
    if (name.find("monitor") != std::string::npos || name.find("htop") != std::string::npos || app.find("monitor") != std::string::npos)
        return "📊";
    if (name.find("setting") != std::string::npos || name.find("prefer") != std::string::npos || app.find("setting") != std::string::npos)
        return "⚙️";
    if (name.find("package") != std::string::npos || name.find("software") != std::string::npos || app.find("package") != std::string::npos)
        return "📦";
    if (name.find("file") != std::string::npos || name.find("folder") != std::string::npos || app.find("file") != std::string::npos)
        return "📂";
    if (name.find("browser") != std::string::npos || name.find("firefox") != std::string::npos || name.find("chrome") != std::string::npos || name.find("web") != std::string::npos || name.find("internet") != std::string::npos)
        return "🌐";
    if (name.find("text") != std::string::npos || name.find("edit") != std::string::npos || name.find("note") != std::string::npos || name.find("document") != std::string::npos)
        return "📄";
    if (name.find("calc") != std::string::npos || app.find("calc") != std::string::npos)
        return "🧮";
    if (name.find("mail") != std::string::npos || name.find("thunderbird") != std::string::npos)
        return "✉️";
    if (name.find("music") != std::string::npos || name.find("player") != std::string::npos || name.find("audio") != std::string::npos || name.find("volume") != std::string::npos)
        return "🎵";
    if (name.find("video") != std::string::npos || name.find("vlc") != std::string::npos || name.find("movie") != std::string::npos)
        return "🎬";
    if (name.find("image") != std::string::npos || name.find("photo") != std::string::npos || name.find("gimp") != std::string::npos || name.find("paint") != std::string::npos)
        return "🎨";
    if (name.find("game") != std::string::npos || name.find("steam") != std::string::npos)
        return "🎮";
    if (name.find("development") != std::string::npos || name.find("code") != std::string::npos || name.find("visual-studio") != std::string::npos || name.find("developer") != std::string::npos)
        return "🛠️";
    
    return "🔹";
}

// ---------------------------------------------------------------------------
// Load applications from system .desktop directories + fallback apps
// ---------------------------------------------------------------------------
static std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;

    // Built-in system defaults
    apps.push_back({"Foot Terminal", "foot", "Fast Wayland Terminal Emulator", true, "utilities-terminal"});
    apps.push_back({"Weston Terminal", "weston-terminal", "Wayland Demo Terminal", true, "utilities-terminal"});
    apps.push_back({"Alacritty", "alacritty", "GPU Accelerated Terminal", true, "utilities-terminal"});
    apps.push_back({"Tinexus System Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", false, "utilities-system-monitor"});
    apps.push_back({"Tinexus Settings", "tinexus-settings", "System Configuration Manager", false, "preferences-desktop"});
    apps.push_back({"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", false, "system-software-install"});
    apps.push_back({"Tinexus Files", "tinexus-files", "Lightweight Desktop File Manager", false, "system-file-manager"});

    // Scan system application directories
    std::vector<fs::path> search_dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };
    const char* home = std::getenv("HOME");
    if (home) {
        search_dirs.push_back(fs::path(home) / ".local" / "share" / "applications");
    }

    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    auto parsed = indexer::DesktopParser::parse_file(entry.path());
                    if (parsed && !parsed->no_display && !parsed->exec.empty()) {
                        // Avoid duplicates by exec or name
                        bool exists = std::any_of(apps.begin(), apps.end(), [&](const AppItem& item) {
                            return item.name == parsed->name || item.exec == parsed->exec;
                        });
                        if (!exists) {
                            apps.push_back({
                                parsed->name,
                                parsed->exec,
                                parsed->comment.empty() ? parsed->generic_name : parsed->comment,
                                parsed->terminal,
                                parsed->icon
                            });
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            log::warn("[Launcher] Exception scanning app dir {}: {}", dir.string(), e.what());
        }
    }

    log::info("[Launcher] Loaded {} application entries", apps.size());
    return apps;
}

int main() {
    log::set_component_name("launcher");
    log::info("[Launcher] Tinexus Command Palette starting...");

    // Prevent zombie child processes
    signal(SIGCHLD, SIG_IGN);

    auto window = txui::Window::create(700, 480, "Tinexus Launcher");
    if (!window || !window->is_wayland_connected()) {
        log::error("[Launcher] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    std::vector<AppItem> all_apps = load_system_apps();
    std::string query = "";
    size_t selected_index = 0;

    // Root layout
    auto root = txui::make_ref<txui::FlexLayout>();
    root->set_direction(txui::FlexDirection::Column);
    root->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    root->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);

    // 1. Header Box
    auto header_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(15, 23, 42, 240)); // #0F172A
    auto header_text = txui::make_ref<txui::TextWidget>(" TINEXUS COMMAND PALETTE ", txui::Color(148, 163, 184, 255), 1.0);
    auto header_box = txui::make_ref<txui::SizedBox>(680, 24);
    header_box->add_child(header_bg);
    header_box->add_child(header_text);

    // 2. Search Box Container
    auto search_bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(30, 41, 59, 240)); // #1E293B
    auto search_text = txui::make_ref<txui::TextWidget>("> Type to search apps...", txui::Color::white(), 1.5);
    auto search_box = txui::make_ref<txui::SizedBox>(680, 48);
    search_box->add_child(search_bg);
    search_box->add_child(search_text);

    // Spacer
    auto spacer = txui::make_ref<txui::SizedBox>(680, 10);

    // 3. Results Container
    constexpr size_t MAX_VISIBLE_ROWS = 6;
    struct RowWidgets {
        txui::Ref<txui::SizedBox> box;
        txui::Ref<txui::SolidColorWidget> bg;
        txui::Ref<txui::TextWidget> text;
    };
    std::vector<RowWidgets> row_widgets;
    row_widgets.reserve(MAX_VISIBLE_ROWS);

    auto results_layout = txui::make_ref<txui::FlexLayout>();
    results_layout->set_direction(txui::FlexDirection::Column);
    results_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    results_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);

    for (size_t i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        auto bg = txui::make_ref<txui::SolidColorWidget>(txui::Color(30, 41, 59, 200));
        auto text = txui::make_ref<txui::TextWidget>("", txui::Color::white(), 1.2);
        auto box = txui::make_ref<txui::SizedBox>(680, 44);
        box->add_child(bg);
        box->add_child(text);

        row_widgets.push_back({box, bg, text});
        results_layout->add_child(box);
    }

    auto results_container = txui::make_ref<txui::SizedBox>(680, 360);
    results_container->add_child(results_layout);

    root->add_child(header_box);
    root->add_child(search_box);
    root->add_child(spacer);
    root->add_child(results_container);

    window->set_root_widget(root);

    // Initial render setup
    auto animation_start_time = std::chrono::steady_clock::now();
    bool is_animating = true;

    // Update results filter & UI elements
    auto update_results_ui = [&]() {
        std::string lq = to_lower(query);
        std::vector<AppItem> filtered;
        for (const auto& app : all_apps) {
            if (lq.empty() ||
                to_lower(app.name).find(lq) != std::string::npos ||
                to_lower(app.exec).find(lq) != std::string::npos ||
                to_lower(app.description).find(lq) != std::string::npos) {
                filtered.push_back(app);
            }
        }

        if (filtered.empty()) {
            selected_index = 0;
        } else if (selected_index >= filtered.size()) {
            selected_index = filtered.size() - 1;
        }

        // Search text update
        if (query.empty()) {
            search_text->set_text("> Type to search apps...");
            search_text->set_color(txui::Color(148, 163, 184, 255));
        } else {
            search_text->set_text("> " + query + "_");
            search_text->set_color(txui::Color::white());
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - animation_start_time).count();
        bool any_row_animating = false;

        // Row updates
        for (size_t i = 0; i < MAX_VISIBLE_ROWS; ++i) {
            // Stagger delay: i * 20ms
            double row_delay = static_cast<double>(i) * 20.0;
            double row_duration = 80.0; // 80ms
            double row_elapsed = static_cast<double>(elapsed) - row_delay;
            double progress = 0.0;
            if (row_elapsed >= row_duration) {
                progress = 1.0;
            } else if (row_elapsed > 0.0) {
                progress = row_elapsed / row_duration;
                any_row_animating = true;
            } else {
                any_row_animating = true; // hasn't started yet
            }

            // Easing curve: cubic ease-out
            double t = progress - 1.0;
            double eased = t * t * t + 1.0;

            // Offset: translateY 4px -> 0px
            double offset_y = 4.0 * (1.0 - eased);
            row_widgets[i].text->set_offset_y(offset_y);

            if (i < filtered.size()) {
                const auto& item = filtered[i];
                std::string emoji = icon_to_emoji(item.icon, item.name);
                if (i == selected_index) {
                    // Highlighted active item
                    row_widgets[i].bg->set_color(txui::Color(59, 130, 246, static_cast<uint8_t>(240 * eased))); // #3B82F6 Accent Blue
                    row_widgets[i].text->set_color(txui::Color(255, 255, 255, static_cast<uint8_t>(255 * eased)));
                    row_widgets[i].text->set_text(" > " + emoji + "  " + item.name + " (" + item.exec + ")");
                } else {
                    // Unselected item
                    row_widgets[i].bg->set_color(txui::Color(30, 41, 59, static_cast<uint8_t>(200 * eased))); // Dark Gray
                    row_widgets[i].text->set_color(txui::Color(203, 213, 225, static_cast<uint8_t>(255 * eased)));
                    row_widgets[i].text->set_text("   " + emoji + "  " + item.name);
                }
            } else {
                // Empty row
                row_widgets[i].bg->set_color(txui::Color(15, 23, 42, static_cast<uint8_t>(100 * eased)));
                row_widgets[i].text->set_text("");
            }
        }

        is_animating = any_row_animating;
    };

    // Initial render
    update_results_ui();

    // Event loop
    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::KeyDown) {
                bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);

                if (event.keyboard.key == txui::Key::Escape) {
                    running = false;
                } else if (event.keyboard.key == txui::Key::Enter) {
                    // Launch highlighted app
                    std::string lq = to_lower(query);
                    std::vector<AppItem> filtered;
                    for (const auto& app : all_apps) {
                        if (lq.empty() ||
                            to_lower(app.name).find(lq) != std::string::npos ||
                            to_lower(app.exec).find(lq) != std::string::npos ||
                            to_lower(app.description).find(lq) != std::string::npos) {
                            filtered.push_back(app);
                        }
                    }

                    if (!filtered.empty() && selected_index < filtered.size()) {
                        const auto& target = filtered[selected_index];
                        log::info("[Launcher] User selected '{}' (exec '{}')", target.name, target.exec);
                        spawn_app(target.exec, target.is_terminal);
                        running = false;
                    } else if (!query.empty()) {
                        // Direct command execution fallback
                        log::info("[Launcher] Direct command launch '{}'", query);
                        spawn_app(query, false);
                        running = false;
                    }
                } else if (event.keyboard.key == txui::Key::Up) {
                    if (selected_index > 0) {
                        selected_index--;
                        animation_start_time = std::chrono::steady_clock::now();
                        is_animating = true;
                        update_results_ui();
                    }
                } else if (event.keyboard.key == txui::Key::Down) {
                    selected_index++;
                    animation_start_time = std::chrono::steady_clock::now();
                    is_animating = true;
                    update_results_ui();
                } else if (event.keyboard.key == txui::Key::Backspace) {
                    if (!query.empty()) {
                        query.pop_back();
                        selected_index = 0;
                        animation_start_time = std::chrono::steady_clock::now();
                        is_animating = true;
                        update_results_ui();
                    }
                } else {
                    char ch = key_to_char(event.keyboard.key, shift);
                    if (ch != '\0') {
                        query += ch;
                        selected_index = 0;
                        animation_start_time = std::chrono::steady_clock::now();
                        is_animating = true;
                        update_results_ui();
                    }
                }
            }
        }

        if (is_animating) {
            update_results_ui();
        }

        window->present();
        if (is_animating) {
            usleep(16666); // ~60fps
        } else {
            window->wait();
        }
    }

    log::info("[Launcher] Exiting launcher loop cleanly.");
    return 0;
}
