#ifndef TINEXUS_FILES_FILE_OPERATIONS_HPP
#define TINEXUS_FILES_FILE_OPERATIONS_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <stop_token>

namespace tinexus::files {

struct OperationResult {
    bool success{false};
    std::string error_message{""};
    std::filesystem::path resulting_path{""};
};

struct CopyProgress {
    uint64_t bytes_transferred{0};
    uint64_t total_bytes{0};
    double progress_fraction{0.0};
    double bytes_per_sec{0.0};
};

enum class ConflictResolution {
    Skip,
    Replace,
    KeepBoth,
    Abort
};

enum class BatchConflictPolicy {
    AskEach,
    SkipAll,
    ReplaceAll,
    KeepBothAll
};

struct ConflictInfo {
    std::filesystem::path source_path;
    std::filesystem::path dest_path;
    uint64_t source_size{0};
    uint64_t dest_size{0};
    int64_t source_mtime{0};
    int64_t dest_mtime{0};
    bool is_directory{false};
    bool is_type_mismatch{false};
    uint64_t dest_dir_item_count{0};
    uint64_t dest_dir_total_size{0};
};

using ProgressCallback = std::function<void(const CopyProgress&)>;
using ConflictCallback = std::function<ConflictResolution(const ConflictInfo&, std::stop_token)>;

class FileOperations {
public:
    // Create new folder with automatic conflict resolution: "New Folder", "New Folder 2", etc.
    static OperationResult create_folder(const std::filesystem::path& parent_dir, const std::string& base_name = "New Folder");

    // Create new empty text file with conflict resolution: "New Document.txt", "New Document 2.txt", etc.
    static OperationResult create_file(const std::filesystem::path& parent_dir, const std::string& base_name = "New Document.txt");

    // Safe rename: renames old_path to new_name in the same directory
    static OperationResult rename_path(const std::filesystem::path& old_path, const std::string& new_name);

    // Safe copy: copies src into dest_dir
    static OperationResult copy_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir);

    // Chunked streaming copy supporting progress callback, collision callback, and std::stop_token cooperative cancellation.
    // Cleans up partial target file if cancelled.
    static OperationResult copy_path_streaming(
        const std::filesystem::path& src,
        const std::filesystem::path& dest_dir,
        std::stop_token stop_token = {},
        ProgressCallback on_progress = nullptr,
        ConflictCallback on_conflict = nullptr
    );

    // Safe move: moves src into dest_dir with collision callback
    static OperationResult move_path(
        const std::filesystem::path& src,
        const std::filesystem::path& dest_dir,
        std::stop_token stop_token = {},
        ProgressCallback on_progress = nullptr,
        ConflictCallback on_conflict = nullptr
    );

    // Safe Trash: strictly moves file/directory to $XDG_DATA_HOME/Trash/files/ (never permanent unlink)
    static OperationResult trash_path(const std::filesystem::path& path);

    // Helper: auto-generate unique non-colliding filename in target directory
    static std::filesystem::path get_unique_destination_path(const std::filesystem::path& dest_dir, const std::string& filename);

    // Helper: calculate total bytes of a file or directory tree
    static uint64_t compute_tree_size(const std::filesystem::path& path, std::stop_token stop_token = {});

    // Helper: count items and total size of a directory (used for type mismatch warning)
    static void compute_dir_stats(const std::filesystem::path& path, uint64_t& out_count, uint64_t& out_size, std::stop_token stop_token = {});
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_FILE_OPERATIONS_HPP

