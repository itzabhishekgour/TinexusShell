#include "files/file_operations.hpp"
#include <fstream>
#include <cstdlib>
#include <system_error>

namespace tinexus::files {

static std::filesystem::path get_trash_dir() {
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    std::filesystem::path trash_base;
    if (xdg_data && *xdg_data) {
        trash_base = std::filesystem::path(xdg_data) / "Trash" / "files";
    } else {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            trash_base = std::filesystem::path(home) / ".local" / "share" / "Trash" / "files";
        } else {
            trash_base = "/tmp/tinexus-trash";
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(trash_base, ec);
    return trash_base;
}

std::filesystem::path FileOperations::get_unique_destination_path(const std::filesystem::path& dest_dir, const std::string& filename) {
    std::filesystem::path candidate = dest_dir / filename;
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    std::filesystem::path file_p(filename);
    std::string stem = file_p.stem().string();
    std::string ext = file_p.extension().string();

    int count = 1;
    while (true) {
        std::string new_name = stem + " (" + std::to_string(count) + ")" + ext;
        candidate = dest_dir / new_name;
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
        count++;
    }
}

OperationResult FileOperations::create_folder(const std::filesystem::path& parent_dir, const std::string& base_name) {
    std::error_code ec;
    if (!std::filesystem::exists(parent_dir, ec) || !std::filesystem::is_directory(parent_dir, ec)) {
        return {false, "Parent directory does not exist or is not a directory.", {}};
    }

    std::filesystem::path target = get_unique_destination_path(parent_dir, base_name);
    if (!std::filesystem::create_directory(target, ec)) {
        return {false, ec.message(), {}};
    }

    return {true, "", target};
}

OperationResult FileOperations::create_file(const std::filesystem::path& parent_dir, const std::string& base_name) {
    std::error_code ec;
    if (!std::filesystem::exists(parent_dir, ec) || !std::filesystem::is_directory(parent_dir, ec)) {
        return {false, "Parent directory does not exist or is not a directory.", {}};
    }

    std::filesystem::path target = get_unique_destination_path(parent_dir, base_name);
    std::ofstream ofs(target);
    if (!ofs.is_open()) {
        return {false, "Failed to create file (permission denied or read-only filesystem).", {}};
    }
    ofs.close();

    return {true, "", target};
}

OperationResult FileOperations::rename_path(const std::filesystem::path& old_path, const std::string& new_name) {
    std::error_code ec;
    if (!std::filesystem::exists(old_path, ec)) {
        return {false, "Source item does not exist.", {}};
    }

    if (new_name.empty() || new_name.find('/') != std::string::npos) {
        return {false, "Invalid file name.", {}};
    }

    std::filesystem::path target = old_path.parent_path() / new_name;
    if (std::filesystem::exists(target, ec)) {
        return {false, "An item with that name already exists.", {}};
    }

    std::filesystem::rename(old_path, target, ec);
    if (ec) {
        return {false, ec.message(), {}};
    }

    return {true, "", target};
}

OperationResult FileOperations::copy_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir) {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        return {false, "Source item does not exist.", {}};
    }
    if (!std::filesystem::is_directory(dest_dir, ec)) {
        return {false, "Destination is not a directory.", {}};
    }

    // Auto-suffix collision resolution (never silent overwrite)
    std::filesystem::path target = get_unique_destination_path(dest_dir, src.filename().string());

    auto opts = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;
    std::filesystem::copy(src, target, opts, ec);
    if (ec) {
        return {false, ec.message(), {}};
    }

    return {true, "", target};
}

OperationResult FileOperations::move_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir) {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        return {false, "Source item does not exist.", {}};
    }
    if (!std::filesystem::is_directory(dest_dir, ec)) {
        return {false, "Destination is not a directory.", {}};
    }

    std::filesystem::path target = get_unique_destination_path(dest_dir, src.filename().string());

    // Try rename first
    std::filesystem::rename(src, target, ec);
    if (ec) {
        // Across filesystem boundaries: copy then delete original
        ec.clear();
        auto opts = std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks;
        std::filesystem::copy(src, target, opts, ec);
        if (ec) {
            return {false, ec.message(), {}};
        }
        std::filesystem::remove_all(src, ec);
    }

    return {true, "", target};
}

OperationResult FileOperations::trash_path(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return {false, "Target item does not exist.", {}};
    }

    // Strictly move to recoverable trash directory - never permanent unlink
    std::filesystem::path trash_dir = get_trash_dir();
    return move_path(path, trash_dir);
}

} // namespace tinexus::files
