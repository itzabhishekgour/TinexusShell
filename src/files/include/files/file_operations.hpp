#ifndef TINEXUS_FILES_FILE_OPERATIONS_HPP
#define TINEXUS_FILES_FILE_OPERATIONS_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace tinexus::files {

struct OperationResult {
    bool success{false};
    std::string error_message{""};
    std::filesystem::path resulting_path{""};
};

class FileOperations {
public:
    // Create new folder with automatic conflict resolution: "New Folder", "New Folder 2", etc.
    static OperationResult create_folder(const std::filesystem::path& parent_dir, const std::string& base_name = "New Folder");

    // Create new empty text file with conflict resolution: "New Document.txt", "New Document 2.txt", etc.
    static OperationResult create_file(const std::filesystem::path& parent_dir, const std::string& base_name = "New Document.txt");

    // Safe rename: renames old_path to new_name in the same directory
    static OperationResult rename_path(const std::filesystem::path& old_path, const std::string& new_name);

    // Safe copy: copies src into dest_dir with auto-suffixing if collision occurs ("file (1).ext")
    static OperationResult copy_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir);

    // Safe move: moves src into dest_dir with auto-suffixing if collision occurs
    static OperationResult move_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir);

    // Safe Trash: strictly moves file/directory to $XDG_DATA_HOME/Trash/files/ (never permanent unlink)
    static OperationResult trash_path(const std::filesystem::path& path);

    // Helper: auto-generate unique non-colliding filename in target directory
    static std::filesystem::path get_unique_destination_path(const std::filesystem::path& dest_dir, const std::string& filename);
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_FILE_OPERATIONS_HPP
