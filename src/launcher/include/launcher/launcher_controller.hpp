#ifndef TINEXUS_LAUNCHER_CONTROLLER_HPP
#define TINEXUS_LAUNCHER_CONTROLLER_HPP

#include "launcher/search_model.hpp"
#include <string>

namespace tinexus::launcher {

class LauncherController {
public:
    static LauncherController& instance() noexcept;

    LauncherController();
    ~LauncherController() = default;

    [[nodiscard]] bool is_visible() const noexcept;
    void show();
    void hide();
    void toggle_visibility();

    void on_search_text_changed(const std::string& query);
    void activate_selected_item(size_t index);

    [[nodiscard]] const SearchModel& model() const noexcept;

private:
    bool m_visible{false};
    SearchModel m_model;
};

} // namespace tinexus::launcher

#endif // TINEXUS_LAUNCHER_CONTROLLER_HPP
