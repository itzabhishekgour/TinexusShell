#pragma once

#include <vector>
#include <filesystem>
#include <algorithm>

namespace tinexus::files {

class RecentManager {
public:
    static RecentManager& instance() {
        static RecentManager s_instance;
        return s_instance;
    }

    void add_recent(const std::filesystem::path& path) {
        auto it = std::find(m_recents.begin(), m_recents.end(), path);
        if (it != m_recents.end()) {
            m_recents.erase(it);
        }
        m_recents.insert(m_recents.begin(), path);
        if (m_recents.size() > 30) {
            m_recents.resize(30);
        }
    }

    [[nodiscard]] const std::vector<std::filesystem::path>& recents() const noexcept {
        return m_recents;
    }

private:
    RecentManager() = default;
    std::vector<std::filesystem::path> m_recents;
};

} // namespace tinexus::files
