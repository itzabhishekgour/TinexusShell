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

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-indexerd");
    tinexus::log::info("Starting tinexus-indexerd v{} - Application & Desktop Entry Indexer Daemon", tinexus::VERSION_STRING);

    // XDG Cache directory setup
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    fs::path db_dir = xdg_cache ? fs::path(xdg_cache) / "tinexus" : fs::path(std::getenv("HOME")) / ".cache" / "tinexus";
    fs::create_directories(db_dir);
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
    const char* home = std::getenv("HOME");
    if (home) {
        app_dirs.push_back(fs::path(home) / ".local" / "share" / "applications");
    }
    app_dirs.push_back("/var/lib/flatpak/exports/share/applications");

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

    // Setup filesystem watcher for incremental updates
    tinexus::indexer::FsWatcher watcher;
    for (const auto& dir : app_dirs) {
        watcher.add_watch_directory(dir);
    }

    std::thread watcher_thread([&watcher, &db]() {
        watcher.start_watching([&db](const fs::path& path, tinexus::indexer::FileChangeType change_type) {
            std::string desktop_id = path.stem().string();
            if (change_type == tinexus::indexer::FileChangeType::Deleted) {
                tinexus::log::info("Incremental update: Removed {}", desktop_id);
                db.remove_entry(desktop_id);
                tinexus::indexer::RamSnapshot::instance().remove_entry(desktop_id);
            } else {
                auto parsed = tinexus::indexer::DesktopParser::parse_file(path);
                if (parsed) {
                    tinexus::log::info("Incremental update: Parsed/Updated {}", parsed->name);
                    db.save_entry(*parsed);
                    tinexus::indexer::RamSnapshot::instance().update_entry(std::move(*parsed));
                }
            }
        });
    });

    tinexus::log::info("Indexerd running. Monitoring desktop entries for changes...");
    if (watcher_thread.joinable()) {
        watcher_thread.join();
    }

    return 0;
}
