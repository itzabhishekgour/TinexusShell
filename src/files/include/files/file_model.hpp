#ifndef TINEXUS_FILES_FILE_MODEL_HPP
#define TINEXUS_FILES_FILE_MODEL_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace tinexus::files {

enum class FileType : uint8_t {
    RegularFile = 0,
    Directory = 1,
    Symlink = 2,
    Executable = 3,
    Unknown = 4
};

struct FileItem {
    std::filesystem::path path;
    std::string name;
    uint64_t size_bytes{0};
    FileType type{FileType::RegularFile};
    std::string mime_type{"application/octet-stream"};
    bool is_hidden{false};
    bool is_executable{false};
    std::filesystem::file_time_type last_modified;
};

class FileModel {
public:
    static std::vector<FileItem> scan_directory(const std::filesystem::path& dir_path, bool show_hidden = false);
    static FileItem stat_file(const std::filesystem::path& file_path);
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_FILE_MODEL_HPP
