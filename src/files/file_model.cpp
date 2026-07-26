#include "files/file_model.hpp"
#include "common/logger.hpp"
#include <system_error>

namespace tinexus::files {

FileItem FileModel::stat_file(const std::filesystem::path& file_path) {
    FileItem item;
    item.path = file_path;
    item.name = file_path.filename().string();
    item.is_hidden = !item.name.empty() && item.name.front() == '.';

    std::error_code ec;
    auto status = std::filesystem::status(file_path, ec);
    if (!ec) {
        if (std::filesystem::is_directory(status)) {
            item.type = FileType::Directory;
            item.mime_type = "inode/directory";
        } else if (std::filesystem::is_symlink(status)) {
            item.type = FileType::Symlink;
            item.mime_type = "inode/symlink";
        } else {
            item.type = FileType::RegularFile;
            item.size_bytes = std::filesystem::file_size(file_path, ec);
            item.mime_type = "application/octet-stream";
        }
    } else {
        item.type = FileType::Unknown;
    }

    return item;
}

std::vector<FileItem> FileModel::scan_directory(const std::filesystem::path& dir_path, bool show_hidden) {
    log::info("FileModel: Scanning directory '{}'", dir_path.string());
    std::vector<FileItem> items;

    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        log::error("FileModel: Directory '{}' does not exist or is not a directory", dir_path.string());
        return items;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        auto item = stat_file(entry.path());
        if (!show_hidden && item.is_hidden) continue;
        items.push_back(item);
    }

    return items;
}

} // namespace tinexus::files
