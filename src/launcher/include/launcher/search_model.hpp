#ifndef TINEXUS_LAUNCHER_SEARCH_MODEL_HPP
#define TINEXUS_LAUNCHER_SEARCH_MODEL_HPP

#include <string>
#include <vector>

namespace tinexus::launcher {

struct LauncherResultItem {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon_name;
    std::string category;
    std::string action_payload;
};

class SearchModel {
public:
    SearchModel() = default;
    ~SearchModel() = default;

    void set_items(std::vector<LauncherResultItem> items);
    void clear();

    [[nodiscard]] size_t count() const noexcept;
    [[nodiscard]] const std::vector<LauncherResultItem>& items() const noexcept;
    [[nodiscard]] const LauncherResultItem* get_item(size_t index) const noexcept;

private:
    std::vector<LauncherResultItem> m_items;
};

} // namespace tinexus::launcher

#endif // TINEXUS_LAUNCHER_SEARCH_MODEL_HPP
