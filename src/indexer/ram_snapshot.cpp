#include "indexer/ram_snapshot.hpp"
#include <mutex>
#include <algorithm>
#include <cctype>

namespace tinexus::indexer {

RamSnapshot& RamSnapshot::instance() noexcept {
    static RamSnapshot s_instance;
    return s_instance;
}

static std::string get_prefix2(std::string_view sv) {
    if (sv.size() < 2) return "";
    std::string res;
    res.reserve(2);
    res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(sv[0]))));
    res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(sv[1]))));
    return res;
}

void RamSnapshot::rebuild_prefix_index_unlocked() {
    m_prefix_index.clear();
    for (const auto& [id, entry] : m_entries) {
        auto add_words = [this, &entry](std::string_view text) {
            if (text.size() < 2) return;
            for (size_t i = 0; i <= text.size() - 2; ++i) {
                std::string pref = get_prefix2(text.substr(i, 2));
                if (!pref.empty()) {
                    auto& vec = m_prefix_index[pref];
                    if (vec.empty() || vec.back() != &entry) {
                        vec.push_back(&entry);
                    }
                }
            }
        };

        add_words(entry.name);
        add_words(entry.desktop_id);
        add_words(entry.exec);
        add_words(entry.generic_name);
    }
}

void RamSnapshot::for_matching_entries(std::string_view query, const std::function<void(const DesktopEntry&)>& callback) const {
    std::shared_lock lock(m_mutex);

    if (query.size() >= 2) {
        std::string pref = get_prefix2(query);
        auto it = m_prefix_index.find(pref);
        if (it != m_prefix_index.end()) {
            for (const auto* entry_ptr : it->second) {
                callback(*entry_ptr);
            }
        }
        return;
    }

    // Fallback only if query < 2 chars (e.g. single character query 'a')
    for (const auto& [id, entry] : m_entries) {
        callback(entry);
    }
}

std::vector<DesktopEntry> RamSnapshot::get_all_entries() const {
    std::shared_lock lock(m_mutex);
    std::vector<DesktopEntry> results;
    results.reserve(m_entries.size());
    for (const auto& [id, entry] : m_entries) {
        results.push_back(entry);
    }
    return results;
}

std::optional<DesktopEntry> RamSnapshot::get_by_id(const std::string& desktop_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_entries.find(desktop_id);
    if (it != m_entries.end()) {
        return it->second;
    }
    return std::nullopt;
}

uint64_t RamSnapshot::generation() const noexcept {
    std::shared_lock lock(m_mutex);
    return m_generation;
}

void RamSnapshot::update_entry(DesktopEntry entry) {
    std::unique_lock lock(m_mutex);
    std::string id = entry.desktop_id;
    m_entries[id] = std::move(entry);
    rebuild_prefix_index_unlocked();
    m_generation++;
}

void RamSnapshot::remove_entry(const std::string& desktop_id) {
    std::unique_lock lock(m_mutex);
    if (m_entries.erase(desktop_id) > 0) {
        rebuild_prefix_index_unlocked();
        m_generation++;
    }
}

void RamSnapshot::set_all_entries(std::vector<DesktopEntry> entries) {
    std::unique_lock lock(m_mutex);
    m_entries.clear();
    for (auto& entry : entries) {
        m_entries[entry.desktop_id] = std::move(entry);
    }
    rebuild_prefix_index_unlocked();
    m_generation++;
}

} // namespace tinexus::indexer
