#ifndef TINEXUS_CLIPBOARD_CLIPBOARD_MANAGER_HPP
#define TINEXUS_CLIPBOARD_CLIPBOARD_MANAGER_HPP

#include "clipboard/clipboard_item.hpp"
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <optional>

namespace tinexus::clipboard {

class ClipboardManager {
public:
    static ClipboardManager& instance() noexcept;

    ClipboardManager() = default;
    ~ClipboardManager() = default;

    ClipboardId add_item(const std::string& content, const std::string& mime_type = "text/plain");
    bool pin_item(ClipboardId id, bool pin);
    bool set_favorite(ClipboardId id, bool favorite);

    [[nodiscard]] std::vector<ClipboardItem> history() const;
    [[nodiscard]] std::vector<ClipboardItem> pinned_items() const;

private:
    std::atomic<ClipboardId> m_next_id{1};
    mutable std::mutex m_mutex;
    std::deque<ClipboardItem> m_history_ring;
    std::vector<ClipboardItem> m_pinned_set;
    size_t m_history_capacity{500};
};

} // namespace tinexus::clipboard

#endif // TINEXUS_CLIPBOARD_CLIPBOARD_MANAGER_HPP
