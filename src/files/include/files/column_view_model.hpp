#ifndef TINEXUS_FILES_COLUMN_VIEW_MODEL_HPP
#define TINEXUS_FILES_COLUMN_VIEW_MODEL_HPP

#include "files/file_model.hpp"
#include <vector>
#include <filesystem>

namespace tinexus::files {

struct ColumnLevel {
    std::filesystem::path directory_path;
    std::vector<FileItem> items;
    int selected_index{-1};
};

class ColumnViewModel {
public:
    ColumnViewModel() = default;
    ~ColumnViewModel() = default;

    void initialize(const std::filesystem::path& root_path);
    bool select_item(size_t column_index, size_t item_index);
    void pop_to_column(size_t column_index);

    [[nodiscard]] const std::vector<ColumnLevel>& columns() const noexcept { return m_columns; }

private:
    std::vector<ColumnLevel> m_columns;
};

} // namespace tinexus::files

#endif // TINEXUS_FILES_COLUMN_VIEW_MODEL_HPP
