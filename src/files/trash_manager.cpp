#include "files/trash_manager.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <chrono>

namespace tinexus::files {

TrashManager& TrashManager::instance() noexcept {
    static TrashManager s_instance;
    return s_instance;
}

std::filesystem::path TrashManager::get_trash_dir() const {
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? std::filesystem::path(home) : std::filesystem::path("/home/user");
    return base / ".local/share/Trash";
}

bool TrashManager::move_to_trash(const std::filesystem::path& file_path) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        log::error("TrashManager: File '{}' does not exist", file_path.string());
        return false;
    }

    auto trash_dir = get_trash_dir();
    auto files_dir = trash_dir / "files";
    auto info_dir = trash_dir / "info";

    std::filesystem::create_directories(files_dir, ec);
    std::filesystem::create_directories(info_dir, ec);

    std::string stem = file_path.stem().string();
    std::string ext = file_path.has_extension() ? file_path.extension().string() : "";
    std::string filename = file_path.filename().string();
    auto target_files_path = files_dir / filename;
    auto target_info_path = info_dir / (filename + ".trashinfo");

    int collision_counter = 1;
    while (std::filesystem::exists(target_files_path, ec) || std::filesystem::exists(target_info_path, ec)) {
        filename = stem + " " + std::to_string(collision_counter) + ext;
        target_files_path = files_dir / filename;
        target_info_path = info_dir / (filename + ".trashinfo");
        collision_counter++;
    }

    // Move file to Trash/files
    std::filesystem::rename(file_path, target_files_path, ec);
    if (ec) {
        // Cross-filesystem fallback (e.g. /tmp tmpfs -> /root ext4)
        ec.clear();
        auto opts = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks | std::filesystem::copy_options::overwrite_existing;
        std::filesystem::copy(file_path, target_files_path, opts, ec);
        if (ec) {
            log::error("TrashManager: Failed to move '{}' to trash files across device: {}", file_path.string(), ec.message());
            return false;
        }
        std::filesystem::remove_all(file_path, ec);
    }

    // Write Freedesktop Trash Specification .trashinfo file
    std::ofstream info_file(target_info_path);
    if (info_file.is_open()) {
        info_file << "[Trash Info]\nPath=" << std::filesystem::absolute(file_path).string() << "\nDeletionDate=";
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
        info_file << time_buf << "\n";
        info_file.close();
    }

    log::info("TrashManager: Moved '{}' to trash -> {}", file_path.string(), target_files_path.string());
    return true;
}

bool TrashManager::restore_from_trash(const std::string& trash_item_name) {
    auto trash_dir = get_trash_dir();
    auto files_path = trash_dir / "files" / trash_item_name;
    auto info_path = trash_dir / "info" / (trash_item_name + ".trashinfo");

    std::error_code ec;
    if (!std::filesystem::exists(files_path, ec)) return false;

    // Read original path from .trashinfo
    std::ifstream info_file(info_path);
    std::string orig_path_str;
    if (info_file.is_open()) {
        std::string line;
        while (std::getline(info_file, line)) {
            if (line.rfind("Path=", 0) == 0) {
                orig_path_str = line.substr(5);
                break;
            }
        }
        info_file.close();
    }

    if (orig_path_str.empty()) return false;

    // Ensure parent directory exists
    std::filesystem::create_directories(std::filesystem::path(orig_path_str).parent_path(), ec);

    std::filesystem::rename(files_path, orig_path_str, ec);
    if (ec) {
        // Cross-filesystem fallback on restore
        ec.clear();
        auto opts = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;
        std::filesystem::copy(files_path, orig_path_str, opts, ec);
        if (ec) {
            log::error("TrashManager: Failed to restore '{}' across device: {}", trash_item_name, ec.message());
            return false;
        }
        std::filesystem::remove_all(files_path, ec);
    }

    std::filesystem::remove(info_path, ec);
    log::info("TrashManager: Restored '{}' from trash to '{}'", trash_item_name, orig_path_str);
    return true;
}

} // namespace tinexus::files
