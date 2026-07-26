#ifndef TINEXUS_INDEXER_INDEX_DB_HPP
#define TINEXUS_INDEXER_INDEX_DB_HPP

#include "indexer/desktop_entry.hpp"
#include <string>
#include <vector>
#include <memory>

struct sqlite3; // Forward declaration

namespace tinexus::indexer {

class IndexDatabase {
public:
    explicit IndexDatabase(std::string db_path);
    ~IndexDatabase();

    IndexDatabase(const IndexDatabase&) = delete;
    IndexDatabase& operator=(const IndexDatabase&) = delete;

    bool open();
    void close();

    bool save_entry(const DesktopEntry& entry);
    bool remove_entry(const std::string& desktop_id);
    bool save_all(const std::vector<DesktopEntry>& entries);

    std::vector<DesktopEntry> load_all_entries();

private:
    std::string m_db_path;
    sqlite3* m_db{nullptr};

    bool init_schema();
};

} // namespace tinexus::indexer

#endif // TINEXUS_INDEXER_INDEX_DB_HPP
