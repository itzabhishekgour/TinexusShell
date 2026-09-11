#include "common/logger.hpp"
#include "common/version.hpp"
#include "indexer/desktop_entry.hpp"
#include "indexer/index_db.hpp"
#include "indexer/ram_snapshot.hpp"
#include "indexer/fs_watcher.hpp"
#include "indexer/ipc_events.hpp"
#include <thread>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    tinexus::log::info("indexerd received signal {}, shutting down...", signal);
    g_running = false;
}
} // namespace

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-indexerd");
    tinexus::log::info("Starting tinexus-indexerd v{} - Application & Desktop Entry Indexer Daemon", tinexus::VERSION_STRING);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // XDG Cache directory setup
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    fs::path db_dir = xdg_cache ? fs::path(xdg_cache) / "tinexus" : fs::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") / ".cache" / "tinexus";
    try {
        fs::create_directories(db_dir);
    } catch (const std::exception& e) {
        tinexus::log::warn("Failed to create db dir {}: {}", db_dir.string(), e.what());
    }
    fs::path db_path = db_dir / "index.db";

    tinexus::indexer::IndexDatabase db(db_path.string());
    if (!db.open()) {
        tinexus::log::error("Failed to initialize index database at {}", db_path.string());
        return 1;
    }

    tinexus::log::info("Database initialized at {}", db_path.string());

    // Application directories to scan & watch
    std::vector<fs::path> app_dirs;
    app_dirs.push_back("/usr/share/applications");
    app_dirs.push_back("/usr/local/share/applications");
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path user_app_dir = fs::path(home) / ".local" / "share" / "applications";
        try {
            fs::create_directories(user_app_dir);
        } catch (const std::exception& e) {
            tinexus::log::warn("Failed to create user app dir {}: {}", user_app_dir.string(), e.what());
        }
        app_dirs.push_back(user_app_dir);
    }

    // Perform initial scan
    std::vector<tinexus::indexer::DesktopEntry> initial_entries;
    for (const auto& dir : app_dirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                auto parsed = tinexus::indexer::DesktopParser::parse_file(entry.path());
                if (parsed) {
                    initial_entries.push_back(std::move(*parsed));
                }
            }
        }
    }

    tinexus::log::info("Indexed {} application desktop entries", initial_entries.size());
    db.save_all(initial_entries);
    tinexus::indexer::RamSnapshot::instance().set_all_entries(initial_entries);

    tinexus::log::info("Indexerd running. Monitoring desktop entries for changes...");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    tinexus::log::info("tinexus-indexerd shutdown complete.");
    return 0;
}
