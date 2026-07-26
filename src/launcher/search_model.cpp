#include "launcher/search_model.hpp"

namespace tinexus::launcher {

void SearchModel::set_items(std::vector<LauncherResultItem> items) {
    m_items = std::move(items);
}

void SearchModel::clear() {
    m_items.clear();
}

size_t SearchModel::count() const noexcept {
    return m_items.size();
}

const std::vector<LauncherResultItem>& SearchModel::items() const noexcept {
    return m_items;
}

const LauncherResultItem* SearchModel::get_item(size_t index) const noexcept {
    if (index < m_items.size()) {
        return &m_items[index];
    }
    return nullptr;
}

} // namespace tinexus::launcher
