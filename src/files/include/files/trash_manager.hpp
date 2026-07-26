#ifndef TINEXUS_FILES_TRASH_MANAGER_HPP
#define TINEXUS_FILES_TRASH_MANAGER_HPP

#include <filesystem>
#include <string>

namespace tinexus::files {

class TrashManager {
public:
    static TrashManager& instance() noexcept;

    TrashManager() = default;
    ~TrashManager() = default;

    bool move_to_trash(const std::filesystem::path& file_path);
    bool restore_from_trash(const std::string& trash_item_name);
    std::filesystem::path get_trash_dir() const;
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_TRASH_MANAGER_HPP
