#include "indexer/index_db.hpp"
#include "common/logger.hpp"
#include <sqlite3.h>
#include <sstream>

namespace tinexus::indexer {

IndexDatabase::IndexDatabase(std::string db_path)
    : m_db_path(std::move(db_path)) {}

IndexDatabase::~IndexDatabase() {
    close();
}

bool IndexDatabase::open() {
    if (m_db) return true;
    int rc = sqlite3_open(m_db_path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        log::error("Failed to open SQLite database at {}: {}", m_db_path, sqlite3_errmsg(m_db));
        return false;
    }
    return init_schema();
}

void IndexDatabase::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool IndexDatabase::init_schema() {
    const char* schema_sql = R"(
        CREATE TABLE IF NOT EXISTS applications (
            desktop_id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            generic_name TEXT,
            comment TEXT,
            exec TEXT NOT NULL,
            icon TEXT,
            categories TEXT,
            keywords TEXT,
            aliases TEXT,
            no_display INTEGER DEFAULT 0,
            terminal INTEGER DEFAULT 0,
            file_path TEXT,
            last_modified INTEGER
        );

        CREATE VIRTUAL TABLE IF NOT EXISTS applications_fts USING fts5(
            desktop_id UNINDEXED,
            name,
            generic_name,
            comment,
            keywords,
            aliases
        );
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, schema_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        log::error("Failed to initialize SQLite schema: {}", err_msg ? err_msg : "Unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool IndexDatabase::save_entry(const DesktopEntry& entry) {
    if (!m_db) return false;

    const char* insert_sql = R"(
        INSERT OR REPLACE INTO applications 
        (desktop_id, name, generic_name, comment, exec, icon, categories, keywords, aliases, no_display, terminal, file_path, last_modified)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string cats, keys, aliases;
    for (const auto& c : entry.categories) cats += c + ";";
    for (const auto& k : entry.keywords) keys += k + ";";
    for (const auto& a : entry.aliases) aliases += a + ";";

    sqlite3_bind_text(stmt, 1, entry.desktop_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entry.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, entry.generic_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, entry.comment.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, entry.exec.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, entry.icon.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, cats.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, keys.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, aliases.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, entry.no_display ? 1 : 0);
    sqlite3_bind_int(stmt, 11, entry.terminal ? 1 : 0);
    sqlite3_bind_text(stmt, 12, entry.file_path.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 13, static_cast<sqlite3_int64>(entry.last_modified_time));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool IndexDatabase::remove_entry(const std::string& desktop_id) {
    if (!m_db) return false;

    const char* del_sql = "DELETE FROM applications WHERE desktop_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, del_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, desktop_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool IndexDatabase::save_all(const std::vector<DesktopEntry>& entries) {
    if (!m_db) return false;
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    for (const auto& entry : entries) {
        save_entry(entry);
    }
    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

std::vector<DesktopEntry> IndexDatabase::load_all_entries() {
    std::vector<DesktopEntry> results;
    if (!m_db) return results;

    const char* select_sql = "SELECT desktop_id, name, generic_name, comment, exec, icon, file_path, no_display, terminal FROM applications;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, select_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DesktopEntry entry;
            entry.desktop_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            entry.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* gen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            if (gen) entry.generic_name = gen;
            const char* comm = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            if (comm) entry.comment = comm;
            entry.exec = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* icon = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (icon) entry.icon = icon;
            const char* fp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            if (fp) entry.file_path = fp;
            entry.no_display = (sqlite3_column_int(stmt, 7) != 0);
            entry.terminal = (sqlite3_column_int(stmt, 8) != 0);

            results.push_back(std::move(entry));
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

} // namespace tinexus::indexer
