#include "files/column_view_model.hpp"
#include "common/logger.hpp"

namespace tinexus::files {

void ColumnViewModel::initialize(const std::filesystem::path& root_path) {
    m_columns.clear();
    ColumnLevel root_level;
    root_level.directory_path = root_path;
    root_level.items = FileModel::scan_directory(root_path);
    root_level.selected_index = -1;
    m_columns.push_back(root_level);
    log::info("ColumnViewModel: Initialized with root path '{}'", root_path.string());
}

bool ColumnViewModel::select_item(size_t column_index, size_t item_index) {
    if (column_index >= m_columns.size()) return false;

    auto& col = m_columns[column_index];
    if (item_index >= col.items.size()) return false;

    col.selected_index = static_cast<int>(item_index);
    const auto& selected_item = col.items[item_index];

    // Pop any columns to the right of column_index
    m_columns.resize(column_index + 1);

    if (selected_item.type == FileType::Directory) {
        ColumnLevel next_col;
        next_col.directory_path = selected_item.path;
        next_col.items = FileModel::scan_directory(selected_item.path);
        next_col.selected_index = -1;
        m_columns.push_back(next_col);
        log::info("ColumnViewModel: Expanded directory column '{}'", selected_item.path.string());
    }

    return true;
}

void ColumnViewModel::pop_to_column(size_t column_index) {
    if (column_index < m_columns.size()) {
        m_columns.resize(column_index + 1);
    }
}

} // namespace tinexus::files
