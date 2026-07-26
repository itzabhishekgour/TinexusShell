#include "files/file_operations.hpp"
#include "common/logger.hpp"
#include <system_error>

namespace tinexus::files {

std::future<bool> FileOperations::copy_async(const std::filesystem::path& src, const std::filesystem::path& dest) {
    return std::async(std::launch::async, [src, dest]() {
        log::info("FileOperations: Copying '{}' to '{}'...", src.string(), dest.string());
        std::error_code ec;
        std::filesystem::copy(src, dest, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            log::error("FileOperations: Failed to copy '{}': {}", src.string(), ec.message());
            return false;
        }
        return true;
    });
}

std::future<bool> FileOperations::move_async(const std::filesystem::path& src, const std::filesystem::path& dest) {
    return std::async(std::launch::async, [src, dest]() {
        log::info("FileOperations: Moving '{}' to '{}'...", src.string(), dest.string());
        std::error_code ec;
        std::filesystem::rename(src, dest, ec);
        if (ec) {
            log::error("FileOperations: Failed to move '{}': {}", src.string(), ec.message());
            return false;
        }
        return true;
    });
}

std::future<bool> FileOperations::delete_async(const std::filesystem::path& path) {
    return std::async(std::launch::async, [path]() {
        log::info("FileOperations: Permanently deleting '{}'...", path.string());
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        if (ec) {
            log::error("FileOperations: Failed to delete '{}': {}", path.string(), ec.message());
            return false;
        }
        return true;
    });
}

} // namespace tinexus::files
