#include "clipboard/clipboard_manager.hpp"
#include "clipboard/privacy_filter.hpp"
#include "common/logger.hpp"

namespace tinexus::clipboard {

ClipboardManager& ClipboardManager::instance() noexcept {
    static ClipboardManager s_instance;
    return s_instance;
}

ClipboardId ClipboardManager::add_item(const std::string& content, const std::string& mime_type) {
    if (PrivacyFilter::is_sensitive(content)) {
        log::warn("ClipboardManager: Sensitive pattern detected in clipboard item! Excluding from history log.");
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Increment copy count if identical content copied again
    for (auto& item : m_history_ring) {
        if (item.content == content) {
            item.copy_count++;
            item.timestamp = std::chrono::system_clock::now();
            log::info("ClipboardManager: Duplicate clipboard item ID={} copy_count incremented to {}", item.id, item.copy_count);
            return item.id;
        }
    }

    ClipboardItem new_item;
    new_item.id = m_next_id.fetch_add(1);
    new_item.content = content;
    new_item.mime_type = mime_type;

    if (mime_type.find("image/") != std::string::npos) {
        new_item.category = MimeCategory::Image;
    } else if (mime_type.find("text/uri-list") != std::string::npos) {
        new_item.category = MimeCategory::File;
    } else {
        new_item.category = MimeCategory::Text;
    }

    m_history_ring.push_front(new_item);
    if (m_history_ring.size() > m_history_capacity) {
        m_history_ring.pop_back();
    }

    log::info("ClipboardManager: Added new clipboard item ID={} (size={} bytes)", new_item.id, content.size());
    return new_item.id;
}

bool ClipboardManager::pin_item(ClipboardId id, bool pin) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (pin) {
        for (const auto& item : m_history_ring) {
            if (item.id == id) {
                auto pinned_copy = item;
                pinned_copy.is_pinned = true;
                m_pinned_set.push_back(pinned_copy);
                log::info("ClipboardManager: Pinned clipboard item ID={}", id);
                return true;
            }
        }
    } else {
        for (auto it = m_pinned_set.begin(); it != m_pinned_set.end(); ++it) {
            if (it->id == id) {
                m_pinned_set.erase(it);
                log::info("ClipboardManager: Unpinned clipboard item ID={}", id);
                return true;
            }
        }
    }
    return false;
}

bool ClipboardManager::set_favorite(ClipboardId id, bool favorite) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& item : m_history_ring) {
        if (item.id == id) {
            item.is_favorite = favorite;
            log::info("ClipboardManager: Set favorite state={} for item ID={}", favorite, id);
            return true;
        }
    }
    return false;
}

std::vector<ClipboardItem> ClipboardManager::history() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<ClipboardItem>(m_history_ring.begin(), m_history_ring.end());
}

std::vector<ClipboardItem> ClipboardManager::pinned_items() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pinned_set;
}

} // namespace tinexus::clipboard
