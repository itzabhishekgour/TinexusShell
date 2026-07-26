#ifndef TINEXUS_INDEXER_RAM_SNAPSHOT_HPP
#define TINEXUS_INDEXER_RAM_SNAPSHOT_HPP

#include "indexer/desktop_entry.hpp"
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include <string_view>

namespace tinexus::indexer {

class RamSnapshot {
public:
    static RamSnapshot& instance() noexcept;

    // Fast indexed traversal using 2-char prefix buckets
    void for_matching_entries(std::string_view query, const std::function<void(const DesktopEntry&)>& callback) const;

    [[nodiscard]] std::vector<DesktopEntry> get_all_entries() const;
    [[nodiscard]] std::optional<DesktopEntry> get_by_id(const std::string& desktop_id) const;
    [[nodiscard]] uint64_t generation() const noexcept;

    // Atomic updates
    void update_entry(DesktopEntry entry);
    void remove_entry(const std::string& desktop_id);
    void set_all_entries(std::vector<DesktopEntry> entries);

private:
    RamSnapshot() = default;
    void rebuild_prefix_index_unlocked();

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, DesktopEntry> m_entries;
    std::unordered_map<std::string, std::vector<const DesktopEntry*>> m_prefix_index;
    uint64_t m_generation{1};
};

} // namespace tinexus::indexer

#endif // TINEXUS_INDEXER_RAM_SNAPSHOT_HPP
