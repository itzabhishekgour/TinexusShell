#ifndef TINEXUS_FILES_FILE_CLIPBOARD_HPP
#define TINEXUS_FILES_FILE_CLIPBOARD_HPP

#include "files/file_operations.hpp"
#include <vector>
#include <filesystem>

namespace tinexus::files {

enum class ClipboardMode {
    None,
    Copy,
    Cut
};

class FileClipboard {
public:
    static FileClipboard& instance() {
        static FileClipboard s_inst;
        return s_inst;
    }

    void copy(const std::vector<std::filesystem::path>& paths) {
        m_paths = paths;
        m_mode = ClipboardMode::Copy;
    }

    void copy_single(const std::filesystem::path& path) {
        m_paths = {path};
        m_mode = ClipboardMode::Copy;
    }

    void cut(const std::vector<std::filesystem::path>& paths) {
        m_paths = paths;
        m_mode = ClipboardMode::Cut;
    }

    void cut_single(const std::filesystem::path& path) {
        m_paths = {path};
        m_mode = ClipboardMode::Cut;
    }

    void clear() {
        m_paths.clear();
        m_mode = ClipboardMode::None;
    }

    [[nodiscard]] bool has_items() const noexcept { return !m_paths.empty(); }
    [[nodiscard]] ClipboardMode mode() const noexcept { return m_mode; }
    [[nodiscard]] const std::vector<std::filesystem::path>& paths() const noexcept { return m_paths; }

    std::vector<OperationResult> paste_into(const std::filesystem::path& dest_dir) {
        std::vector<OperationResult> results;
        if (!has_items()) return results;

        for (const auto& p : m_paths) {
            if (m_mode == ClipboardMode::Copy) {
                results.push_back(FileOperations::copy_path(p, dest_dir));
            } else if (m_mode == ClipboardMode::Cut) {
                results.push_back(FileOperations::move_path(p, dest_dir));
            }
        }

        if (m_mode == ClipboardMode::Cut) {
            clear();
        }
        return results;
    }

private:
    FileClipboard() = default;
    ~FileClipboard() = default;

    std::vector<std::filesystem::path> m_paths;
    ClipboardMode m_mode{ClipboardMode::None};
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_FILE_CLIPBOARD_HPP
