#ifndef TINEXUS_FILES_FILE_OPERATIONS_HPP
#define TINEXUS_FILES_FILE_OPERATIONS_HPP

#include <filesystem>
#include <future>
#include <string>

namespace tinexus::files {

class FileOperations {
public:
    static std::future<bool> copy_async(const std::filesystem::path& src, const std::filesystem::path& dest);
    static std::future<bool> move_async(const std::filesystem::path& src, const std::filesystem::path& dest);
    static std::future<bool> delete_async(const std::filesystem::path& path);
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_FILE_OPERATIONS_HPP
