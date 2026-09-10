#include <launcher/LauncherWidget.hpp>
#include <txui/window/Window.hpp>
#include <txui/core/SingleInstance.hpp>
#include <common/logger.hpp>
#include <indexer/desktop_entry.hpp>
#include <common/dbus_power.hpp>
#include <unistd.h>   // fork, execvp, execlp, setsid
#include <csignal>    // signal, SIGCHLD, SIG_IGN
#include <cstdlib>    // _exit
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

using namespace tinexus;
using namespace tinexus::launcher;
namespace fs = std::filesystem;

// Recent launches — capped at 5, persisted in-memory per session
static std::vector<AppItem> g_recent_launches;
constexpr size_t MAX_RECENT = 5;

static pid_t spawn_app(const AppItem& item) {
    const std::string& raw_exec = item.exec;
    if (item.kind == ResultKind::System) {
        const std::string& cmd = raw_exec;
        if (cmd == "lock") {
            log::info("[Launcher] System action: Lock Screen");
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execlp("tinexus-lock", "tinexus-lock", nullptr);
                _exit(127);
            }
            return pid;
        } else if (cmd == "shutdown") {
            log::info("[Launcher] System action: Shutdown");
            tinexus::common::dbus_power::poweroff();
            return -1;
        } else if (cmd == "reboot") {
            log::info("[Launcher] System action: Restart");
            tinexus::common::dbus_power::reboot();
            return -1;
        } else if (cmd == "sleep") {
            log::info("[Launcher] System action: Sleep");
            tinexus::common::dbus_power::suspend();
            return -1;
        } else if (cmd == "logout") {
            log::info("[Launcher] System action: Log Out");
            tinexus::common::dbus_power::logout();
            return -1;
        }
        return -1;
    }

    std::string clean_exec = indexer::DesktopParser::sanitize_exec(raw_exec);
    if (clean_exec.empty()) return -1;

    // Check single-instance applications before blind fork
    std::string canonical_app_id = txui::get_canonical_app_id(clean_exec);
    if (txui::is_single_instance_app(canonical_app_id) && txui::SingleInstance::is_app_running(canonical_app_id)) {
        log::info("[Launcher] App '{}' is already running; raising existing window", canonical_app_id);
        txui::SingleInstance::focus_app(canonical_app_id);
        return 0;
    }

    // Track recent launch
    auto it = std::find_if(g_recent_launches.begin(), g_recent_launches.end(),
        [&](const AppItem& a) { return a.exec == item.exec; });
    if (it != g_recent_launches.end()) g_recent_launches.erase(it);
    g_recent_launches.insert(g_recent_launches.begin(), item);
    if (g_recent_launches.size() > MAX_RECENT) g_recent_launches.pop_back();

    pid_t pid = fork();
    if (pid < 0) {
        log::error("[Launcher] fork() failed for app '{}'", clean_exec);
        return -1;
    }
    if (pid == 0) {
        setsid();
        if (item.is_terminal) {
            execlp("foot", "foot", "-e", clean_exec.c_str(), nullptr);
            execlp("weston-terminal", "weston-terminal", nullptr);
            _exit(127);
        } else {
            std::vector<std::string> tokens;
            std::istringstream iss(clean_exec);
            std::string token;
            while (iss >> token) tokens.push_back(token);
            if (tokens.empty()) _exit(1);
            std::vector<char*> args;
            for (auto& t : tokens) args.push_back(const_cast<char*>(t.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            _exit(127);
        }
    }
    log::info("[Launcher] Spawned '{}' PID={}", clean_exec, pid);
    return pid;
}

static std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;

    // Built-in system defaults
    apps.push_back({"Tinexus Terminal", "tinexus-terminal", "Default Wayland Terminal", true, "utilities-terminal", ResultKind::App});
    apps.push_back({"Activity Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", false, "utilities-system-monitor", ResultKind::App});
    apps.push_back({"Tinexus Settings", "tinexus-settings-ui", "System Configuration & Control Center", false, "preferences-desktop", ResultKind::App});
    apps.push_back({"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", false, "system-software-install", ResultKind::App});
    apps.push_back({"Tinexus Files", "tinexus-files", "Lightweight Desktop File Manager", false, "system-file-manager", ResultKind::App});

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
                        bool exists = std::any_of(apps.begin(), apps.end(), [&](const AppItem& item) {
                            return item.name == parsed->name || item.exec == parsed->exec;
                        });
                        if (!exists) {
                            apps.push_back({
                                parsed->name,
                                parsed->exec,
                                parsed->comment.empty() ? parsed->generic_name : parsed->comment,
                                parsed->terminal,
                                parsed->icon,
                                ResultKind::App
                            });
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            log::warn("[Launcher] Exception scanning app dir {}: {}", dir.string(), e.what());
        }
    }
    
    return apps;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    log::set_component_name("launcher");

    txui::SingleInstance single_instance("tinexus-launcher");
    if (!single_instance.is_primary()) {
        single_instance.request_focus_primary();
        return 0;
    }

    log::info("[Launcher] Tinexus Command Palette starting...");
    signal(SIGCHLD, SIG_IGN);

    auto window = txui::Window::create(900, 700, "Tinexus Launcher", false, "tinexus-launcher");
    if (!window || !window->is_wayland_connected()) {
        log::error("[Launcher] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    std::vector<AppItem> all_apps = load_system_apps();
    auto launcher_widget = txui::make_ref<LauncherWidget>();
    launcher_widget->set_all_apps(all_apps);
    launcher_widget->set_recent_launches(g_recent_launches);

    bool running = true;

    launcher_widget->set_on_launch([&](const AppItem& item) {
        spawn_app(item);
        running = false;
    });

    launcher_widget->set_on_close([&]() {
        running = false;
    });

    window->set_root_widget(launcher_widget);
    window->present();

    while (running && !window->should_close()) {
        txui::Event event;
        bool needs_redraw = false;

        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else {
                if (launcher_widget->handle_event(event)) {
                    needs_redraw = true;
                }
            }
        }

        if (!running) break;

        if (needs_redraw) {
            window->present();
        }

        window->wait_timeout(16);
    }

    log::info("[Launcher] Exiting cleanly.");
    return 0;
}
