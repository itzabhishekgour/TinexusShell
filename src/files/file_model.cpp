#include "files/file_model.hpp"
#include "common/logger.hpp"
#include <system_error>
#include <algorithm>
#include <cctype>

namespace tinexus::files {

FileItem FileModel::stat_file(const std::filesystem::path& file_path) {
    FileItem item;
    item.path = file_path;
    item.name = file_path.filename().string();
    if (item.name.empty() && file_path == "/") {
        item.name = "/";
    }
    item.is_hidden = !item.name.empty() && item.name.front() == '.' && item.name != "." && item.name != "..";

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
            auto perms = status.permissions();
            bool exec_perm = (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ||
                             (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ||
                             (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
            
            std::string ext = file_path.extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            bool is_known_data_ext = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".svg" || ext == ".webp" ||
                                      ext == ".txt" || ext == ".md" || ext == ".log" || ext == ".toml" || ext == ".json" || ext == ".conf" ||
                                      ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" || ext == ".py" ||
                                      ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".xz" || ext == ".iso" || ext == ".pdf");

            if (!is_known_data_ext && (ext == ".txapp" || ext == ".sh" || exec_perm)) {
                item.is_executable = true;
                item.type = FileType::Executable;
            } else {
                item.type = FileType::RegularFile;
            }

            item.size_bytes = std::filesystem::file_size(file_path, ec);

            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".svg" || ext == ".webp") {
                item.mime_type = "image/" + (ext == ".svg" ? "svg+xml" : (ext.empty() ? "generic" : ext.substr(1)));
            } else if (ext == ".txt" || ext == ".md" || ext == ".log" || ext == ".toml" || ext == ".json" || ext == ".conf") {
                item.mime_type = "text/plain";
            } else if (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" || ext == ".py" || ext == ".sh") {
                item.mime_type = "text/x-source";
            } else if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".xz" || ext == ".iso") {
                item.mime_type = "application/archive";
            } else if (ext == ".pdf") {
                item.mime_type = "application/pdf";
            } else if (item.is_executable) {
                item.mime_type = "application/x-executable";
            } else {
                item.mime_type = "application/octet-stream";
            }
            item.last_modified = std::filesystem::last_write_time(file_path, ec);
        }
    } else {
        item.type = FileType::Unknown;
    }

    return item;
}

void FileModel::sort_items(std::vector<FileItem>& items, SortCriteria criteria, SortDirection direction) {
    std::sort(items.begin(), items.end(), [criteria, direction](const FileItem& a, const FileItem& b) {
        if ((a.type == FileType::Directory) != (b.type == FileType::Directory)) {
            return a.type == FileType::Directory;
        }

        bool result = false;
        switch (criteria) {
            case SortCriteria::Name: {
                std::string a_lower = a.name;
                std::string b_lower = b.name;
                std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), [](unsigned char c){ return std::tolower(c); });
                std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), [](unsigned char c){ return std::tolower(c); });
                result = (a_lower < b_lower);
                break;
            }
            case SortCriteria::DateModified:
                result = (a.last_modified < b.last_modified);
                break;
            case SortCriteria::Size:
                result = (a.size_bytes < b.size_bytes);
                break;
            case SortCriteria::Kind:
                if (a.mime_type != b.mime_type) {
                    result = (a.mime_type < b.mime_type);
                } else {
                    std::string a_lower = a.name;
                    std::string b_lower = b.name;
                    std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), [](unsigned char c){ return std::tolower(c); });
                    std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), [](unsigned char c){ return std::tolower(c); });
                    result = (a_lower < b_lower);
                }
                break;
        }

        return (direction == SortDirection::Ascending) ? result : !result;
    });
}

std::vector<FileItem> FileModel::scan_directory(const std::filesystem::path& dir_path, bool show_hidden) {
    log::info("FileModel: Scanning directory '{}'", dir_path.string());
    std::vector<FileItem> items;

    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        log::error("FileModel: Directory '{}' does not exist or is not a directory", dir_path.string());
        return items;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir_path, std::filesystem::directory_options::skip_permission_denied, ec)) {
        auto item = stat_file(entry.path());
        if (!show_hidden && item.is_hidden) continue;
        items.push_back(item);
    }

    sort_items(items, SortCriteria::Name, SortDirection::Ascending);
    return items;
}

} // namespace tinexus::files
