#include "title/local_content_database.h"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "app_paths.h"
#include "local_catalog_signature.h"
#include "local_sqlite.h"
#include "sqlite3.h"

namespace local_content_database {
namespace {

int origin_to_int(local_content_binding::origin origin) {
    switch (origin) {
    case local_content_binding::origin::downloaded:
        return 1;
    case local_content_binding::origin::linked:
        return 2;
    case local_content_binding::origin::owned_upload:
    default:
        return 0;
    }
}

local_content_binding::origin origin_from_int(int value) {
    switch (value) {
    case 1:
        return local_content_binding::origin::downloaded;
    case 2:
        return local_content_binding::origin::linked;
    case 0:
    default:
        return local_content_binding::origin::owned_upload;
    }
}

local_content_binding::origin merge_origin(local_content_binding::origin current,
                                                  local_content_binding::origin incoming) {
    if (current == local_content_binding::origin::owned_upload) {
        return current;
    }
    if (current == local_content_binding::origin::downloaded &&
        incoming == local_content_binding::origin::linked) {
        return current;
    }
    return incoming;
}

std::int64_t now_unix_seconds() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

using local_sqlite::bind_text;
using local_sqlite::column_text;
using local_sqlite::exec;
using local_sqlite::statement;
using local_sqlite::step_done;

std::mutex g_prune_cache_mutex;
std::optional<std::string> g_last_pruned_catalog_signature;

void invalidate_prune_cache() {
    std::lock_guard<std::mutex> lock(g_prune_cache_mutex);
    g_last_pruned_catalog_signature.reset();
}

bool chart_bindings_has_local_song_id(sqlite3* database) {
    statement columns(database, "PRAGMA table_info(chart_bindings);");
    if (!columns.valid()) {
        return false;
    }
    while (sqlite3_step(columns.get()) == SQLITE_ROW) {
        if (column_text(columns.get(), 1) == "local_song_id") {
            return true;
        }
    }
    return false;
}

bool table_has_column(sqlite3* database, const char* table_name, const char* column_name) {
    const std::string sql = std::string("PRAGMA table_info(") + table_name + ");";
    statement columns(database, sql.c_str());
    if (!columns.valid()) {
        return false;
    }
    while (sqlite3_step(columns.get()) == SQLITE_ROW) {
        if (column_text(columns.get(), 1) == column_name) {
            return true;
        }
    }
    return false;
}

bool table_exists(sqlite3* database, const char* table_name) {
    statement query(database,
                    "SELECT 1 FROM sqlite_master "
                    "WHERE type = 'table' AND name = ?;");
    if (!query.valid()) {
        return false;
    }
    bind_text(query.get(), 1, table_name);
    return sqlite3_step(query.get()) == SQLITE_ROW;
}

std::optional<std::string> ready_catalog_signature(sqlite3* database) {
    const std::optional<std::string> signature =
        local_sqlite::metadata_value(database, "local_catalog.signature");
    if (!signature.has_value() ||
        local_sqlite::metadata_value(database, "local_catalog.status_schema").value_or("") !=
            local_catalog_signature::kStatusSchema ||
        !table_exists(database, "local_songs") ||
        !table_exists(database, "local_charts")) {
        return std::nullopt;
    }
    return signature;
}

struct catalog_cache_ids {
    std::vector<std::pair<std::string, std::string>> songs;
    std::vector<std::pair<std::string, std::string>> charts;
};

std::optional<catalog_cache_ids> load_ready_catalog_cache() {
    if (!std::filesystem::exists(app_paths::local_catalog_cache_db_path())) {
        return std::nullopt;
    }

    local_sqlite::database catalog_database = local_sqlite::open_local_catalog_cache_database();
    if (!catalog_database.valid()) {
        return std::nullopt;
    }
    const std::optional<std::string> signature = ready_catalog_signature(catalog_database.get());
    if (!signature.has_value() || *signature != local_catalog_signature::current()) {
        return std::nullopt;
    }

    catalog_cache_ids ids;
    statement songs(catalog_database.get(), "SELECT song_id, storage_policy FROM local_songs;");
    if (!songs.valid()) {
        return std::nullopt;
    }
    while (sqlite3_step(songs.get()) == SQLITE_ROW) {
        ids.songs.emplace_back(column_text(songs.get(), 0), column_text(songs.get(), 1));
    }

    statement charts(catalog_database.get(), "SELECT chart_id, storage_policy FROM local_charts;");
    if (!charts.valid()) {
        return std::nullopt;
    }
    while (sqlite3_step(charts.get()) == SQLITE_ROW) {
        ids.charts.emplace_back(column_text(charts.get(), 0), column_text(charts.get(), 1));
    }
    return ids;
}

bool seed_live_catalog_tables(sqlite3* database, const catalog_cache_ids& ids) {
    if (!exec(database, "DROP TABLE IF EXISTS temp.live_local_songs;") ||
        !exec(database, "DROP TABLE IF EXISTS temp.live_local_charts;") ||
        !exec(database,
              "CREATE TEMP TABLE live_local_songs("
              "song_id TEXT PRIMARY KEY,"
              "storage_policy TEXT NOT NULL"
              ");") ||
        !exec(database,
              "CREATE TEMP TABLE live_local_charts("
              "chart_id TEXT PRIMARY KEY,"
              "storage_policy TEXT NOT NULL"
              ");")) {
        return false;
    }

    statement insert_song(database,
                          "INSERT OR REPLACE INTO live_local_songs(song_id, storage_policy) "
                          "VALUES(?, ?);");
    if (!insert_song.valid()) {
        return false;
    }
    for (const auto& [song_id, storage_policy] : ids.songs) {
        sqlite3_reset(insert_song.get());
        sqlite3_clear_bindings(insert_song.get());
        bind_text(insert_song.get(), 1, song_id);
        bind_text(insert_song.get(), 2, storage_policy);
        if (!step_done(insert_song.get())) {
            return false;
        }
    }

    statement insert_chart(database,
                           "INSERT OR REPLACE INTO live_local_charts(chart_id, storage_policy) "
                           "VALUES(?, ?);");
    if (!insert_chart.valid()) {
        return false;
    }
    for (const auto& [chart_id, storage_policy] : ids.charts) {
        sqlite3_reset(insert_chart.get());
        sqlite3_clear_bindings(insert_chart.get());
        bind_text(insert_chart.get(), 1, chart_id);
        bind_text(insert_chart.get(), 2, storage_policy);
        if (!step_done(insert_chart.get())) {
            return false;
        }
    }

    return true;
}

bool chart_bindings_has_remote_chart_version(sqlite3* database) {
    return table_has_column(database, "chart_bindings", "remote_chart_version");
}

std::optional<bool> column_optional_bool(sqlite3_stmt* statement, int index) {
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int(statement, index) != 0;
}

void bind_optional_bool(sqlite3_stmt* statement, int index, const std::optional<bool>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(statement, index, *value ? 1 : 0);
    } else {
        sqlite3_bind_null(statement, index);
    }
}

bool migrate_chart_bindings_without_local_song_id(sqlite3* database) {
    if (!chart_bindings_has_local_song_id(database)) {
        return true;
    }

    local_sqlite::transaction tx(database);
    return tx.active() &&
           exec(database, "DROP INDEX IF EXISTS idx_chart_bindings_remote;") &&
           exec(database,
                "CREATE TABLE chart_bindings_new ("
                "server_url TEXT NOT NULL,"
                "local_chart_id TEXT NOT NULL,"
                "remote_chart_id TEXT NOT NULL,"
                "remote_song_id TEXT NOT NULL,"
                "remote_chart_version INTEGER NOT NULL DEFAULT 0,"
                "origin INTEGER NOT NULL,"
                "can_edit INTEGER,"
                "lifecycle_status TEXT NOT NULL DEFAULT '',"
                "review_status TEXT NOT NULL DEFAULT '',"
                "updated_at INTEGER NOT NULL,"
                "PRIMARY KEY (server_url, local_chart_id)"
                ");") &&
           exec(database,
                "INSERT OR REPLACE INTO chart_bindings_new("
                "server_url, local_chart_id, remote_chart_id, remote_song_id, remote_chart_version, "
                "origin, can_edit, lifecycle_status, review_status, updated_at) "
                "SELECT server_url, local_chart_id, remote_chart_id, remote_song_id, 0, "
                "origin, NULL, '', '', updated_at "
                "FROM chart_bindings;") &&
           exec(database, "DROP TABLE chart_bindings;") &&
           exec(database, "ALTER TABLE chart_bindings_new RENAME TO chart_bindings;") &&
           exec(database,
                "CREATE INDEX idx_chart_bindings_remote "
                "ON chart_bindings(server_url, remote_chart_id);") &&
           tx.commit();
}

bool ensure_chart_binding_version_column(sqlite3* database) {
    return chart_bindings_has_remote_chart_version(database) ||
           exec(database, "ALTER TABLE chart_bindings ADD COLUMN remote_chart_version INTEGER NOT NULL DEFAULT 0;");
}

bool ensure_binding_permission_columns(sqlite3* database, const char* table_name) {
    if (!table_has_column(database, table_name, "can_edit") &&
        !exec(database, (std::string("ALTER TABLE ") + table_name + " ADD COLUMN can_edit INTEGER;").c_str())) {
        return false;
    }
    if (!table_has_column(database, table_name, "lifecycle_status") &&
        !exec(database, (std::string("ALTER TABLE ") + table_name +
                        " ADD COLUMN lifecycle_status TEXT NOT NULL DEFAULT '';").c_str())) {
        return false;
    }
    if (!table_has_column(database, table_name, "review_status") &&
        !exec(database, (std::string("ALTER TABLE ") + table_name +
                        " ADD COLUMN review_status TEXT NOT NULL DEFAULT '';").c_str())) {
        return false;
    }
    return true;
}

bool index_is_unique(sqlite3* database, const char* table_name, const char* index_name) {
    const std::string sql = std::string("PRAGMA index_list(") + table_name + ");";
    statement indexes(database, sql.c_str());
    if (!indexes.valid()) {
        return false;
    }
    while (sqlite3_step(indexes.get()) == SQLITE_ROW) {
        if (column_text(indexes.get(), 1) == index_name) {
            return sqlite3_column_int(indexes.get(), 2) != 0;
        }
    }
    return false;
}

bool ensure_non_unique_remote_binding_indexes(sqlite3* database) {
    if (index_is_unique(database, "song_bindings", "idx_song_bindings_remote") &&
        !exec(database, "DROP INDEX idx_song_bindings_remote;")) {
        return false;
    }
    if (index_is_unique(database, "chart_bindings", "idx_chart_bindings_remote") &&
        !exec(database, "DROP INDEX idx_chart_bindings_remote;")) {
        return false;
    }
    return exec(database,
                "CREATE INDEX IF NOT EXISTS idx_song_bindings_remote "
                "ON song_bindings(server_url, remote_song_id);") &&
           exec(database,
                "CREATE INDEX IF NOT EXISTS idx_chart_bindings_remote "
                "ON chart_bindings(server_url, remote_chart_id);");
}

bool remove_legacy_catalog_cache(sqlite3* database) {
    return exec(database, "DROP TABLE IF EXISTS local_charts;") &&
           exec(database, "DROP TABLE IF EXISTS local_songs;") &&
           exec(database,
                "DELETE FROM metadata "
                "WHERE key IN ('local_catalog.signature', 'local_catalog.status_schema');");
}

bool ensure_schema(sqlite3* database) {
    return local_sqlite::ensure_common_schema(database) &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS song_bindings ("
                "server_url TEXT NOT NULL,"
                "local_song_id TEXT NOT NULL,"
                "remote_song_id TEXT NOT NULL,"
                "origin INTEGER NOT NULL,"
                "can_edit INTEGER,"
                "lifecycle_status TEXT NOT NULL DEFAULT '',"
                "review_status TEXT NOT NULL DEFAULT '',"
                "updated_at INTEGER NOT NULL,"
                "PRIMARY KEY (server_url, local_song_id)"
                ");") &&
           exec(database,
                "CREATE INDEX IF NOT EXISTS idx_song_bindings_remote "
                "ON song_bindings(server_url, remote_song_id);") &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS chart_bindings ("
                "server_url TEXT NOT NULL,"
                "local_chart_id TEXT NOT NULL,"
                "remote_chart_id TEXT NOT NULL,"
                "remote_song_id TEXT NOT NULL,"
                "remote_chart_version INTEGER NOT NULL DEFAULT 0,"
                "origin INTEGER NOT NULL,"
                "can_edit INTEGER,"
                "lifecycle_status TEXT NOT NULL DEFAULT '',"
                "review_status TEXT NOT NULL DEFAULT '',"
                "updated_at INTEGER NOT NULL,"
                "PRIMARY KEY (server_url, local_chart_id)"
                ");") &&
           exec(database,
                "CREATE INDEX IF NOT EXISTS idx_chart_bindings_remote "
                "ON chart_bindings(server_url, remote_chart_id);") &&
           migrate_chart_bindings_without_local_song_id(database) &&
           ensure_chart_binding_version_column(database) &&
           ensure_binding_permission_columns(database, "song_bindings") &&
           ensure_binding_permission_columns(database, "chart_bindings") &&
           ensure_non_unique_remote_binding_indexes(database) &&
           remove_legacy_catalog_cache(database) &&
           exec(database,
                "INSERT INTO metadata(key, value) VALUES('schema_version', '5') "
                "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
}

std::optional<local_content_binding::origin> current_song_origin(sqlite3* database,
                                                                        const std::string& server_url,
                                                                        const std::string& local_song_id) {
    statement query(database,
                    "SELECT origin FROM song_bindings WHERE server_url = ? AND local_song_id = ?;");
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, local_song_id);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return origin_from_int(sqlite3_column_int(query.get(), 0));
}

std::optional<local_content_binding::origin> current_chart_origin(sqlite3* database,
                                                                         const std::string& server_url,
                                                                         const std::string& local_chart_id) {
    statement query(database,
                    "SELECT origin FROM chart_bindings WHERE server_url = ? AND local_chart_id = ?;");
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, local_chart_id);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return origin_from_int(sqlite3_column_int(query.get(), 0));
}

void put_song(sqlite3* database, const local_content_binding::song_binding& binding) {
    if (binding.server_url.empty() || binding.local_song_id.empty() || binding.remote_song_id.empty()) {
        return;
    }

    const std::optional<local_content_binding::origin> current_origin =
        current_song_origin(database, binding.server_url, binding.local_song_id);
    const local_content_binding::origin origin = current_origin.has_value()
        ? merge_origin(*current_origin, binding.origin)
        : binding.origin;

    statement query(database,
                    "INSERT INTO song_bindings(server_url, local_song_id, remote_song_id, origin, "
                    "can_edit, lifecycle_status, review_status, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(server_url, local_song_id) DO UPDATE SET "
                    "remote_song_id = excluded.remote_song_id,"
                    "origin = excluded.origin,"
                    "can_edit = excluded.can_edit,"
                    "lifecycle_status = excluded.lifecycle_status,"
                    "review_status = excluded.review_status,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, binding.server_url);
    bind_text(query.get(), 2, binding.local_song_id);
    bind_text(query.get(), 3, binding.remote_song_id);
    sqlite3_bind_int(query.get(), 4, origin_to_int(origin));
    bind_optional_bool(query.get(), 5, binding.can_edit);
    bind_text(query.get(), 6, binding.lifecycle_status);
    bind_text(query.get(), 7, binding.review_status);
    sqlite3_bind_int64(query.get(), 8, now_unix_seconds());
    step_done(query.get());
}

void put_chart(sqlite3* database, const local_content_binding::chart_binding& binding) {
    if (binding.server_url.empty() || binding.local_chart_id.empty() ||
        binding.remote_chart_id.empty() || binding.remote_song_id.empty()) {
        return;
    }

    const std::optional<local_content_binding::origin> current_origin =
        current_chart_origin(database, binding.server_url, binding.local_chart_id);
    const local_content_binding::origin origin = current_origin.has_value()
        ? merge_origin(*current_origin, binding.origin)
        : binding.origin;

    statement query(database,
                    "INSERT INTO chart_bindings(server_url, local_chart_id, remote_chart_id, "
                    "remote_song_id, remote_chart_version, origin, can_edit, lifecycle_status, review_status, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(server_url, local_chart_id) DO UPDATE SET "
                    "remote_chart_id = excluded.remote_chart_id,"
                    "remote_song_id = excluded.remote_song_id,"
                    "remote_chart_version = excluded.remote_chart_version,"
                    "origin = excluded.origin,"
                    "can_edit = excluded.can_edit,"
                    "lifecycle_status = excluded.lifecycle_status,"
                    "review_status = excluded.review_status,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, binding.server_url);
    bind_text(query.get(), 2, binding.local_chart_id);
    bind_text(query.get(), 3, binding.remote_chart_id);
    bind_text(query.get(), 4, binding.remote_song_id);
    sqlite3_bind_int(query.get(), 5, binding.remote_chart_version);
    sqlite3_bind_int(query.get(), 6, origin_to_int(origin));
    bind_optional_bool(query.get(), 7, binding.can_edit);
    bind_text(query.get(), 8, binding.lifecycle_status);
    bind_text(query.get(), 9, binding.review_status);
    sqlite3_bind_int64(query.get(), 10, now_unix_seconds());
    step_done(query.get());
}

bool prune_orphaned_bindings(sqlite3* database) {
    const std::optional<catalog_cache_ids> catalog_ids = load_ready_catalog_cache();
    if (!catalog_ids.has_value()) {
        return true;
    }
    const std::string signature = local_catalog_signature::current();

    std::lock_guard<std::mutex> lock(g_prune_cache_mutex);
    if (g_last_pruned_catalog_signature == signature) {
        return true;
    }

    local_sqlite::transaction tx(database);
    const bool pruned =
        tx.active() &&
        seed_live_catalog_tables(database, *catalog_ids) &&
        exec(database,
             "DELETE FROM chart_bindings "
             "WHERE origin = 1 "
             "AND EXISTS ("
             "SELECT 1 FROM live_local_charts "
             "WHERE live_local_charts.chart_id = chart_bindings.local_chart_id "
             "AND live_local_charts.storage_policy = 'plain_workspace'"
             ");") &&
        exec(database,
             "DELETE FROM song_bindings "
             "WHERE origin = 1 "
             "AND EXISTS ("
             "SELECT 1 FROM live_local_songs "
             "WHERE live_local_songs.song_id = song_bindings.local_song_id "
             "AND live_local_songs.storage_policy = 'plain_workspace'"
             ");") &&
        exec(database,
             "DELETE FROM chart_bindings "
             "WHERE NOT EXISTS ("
             "SELECT 1 FROM live_local_charts "
             "WHERE live_local_charts.chart_id = chart_bindings.local_chart_id"
             ");") &&
        exec(database,
             "DELETE FROM song_bindings "
             "WHERE NOT EXISTS ("
             "SELECT 1 FROM live_local_songs "
             "WHERE live_local_songs.song_id = song_bindings.local_song_id"
             ");") &&
        exec(database,
             "DELETE FROM chart_bindings "
             "WHERE NOT EXISTS ("
             "SELECT 1 FROM song_bindings "
             "WHERE song_bindings.server_url = chart_bindings.server_url "
             "AND song_bindings.remote_song_id = chart_bindings.remote_song_id"
             ");") &&
        tx.commit();
    if (pruned) {
        g_last_pruned_catalog_signature = signature;
    }
    return pruned;
}

local_sqlite::database open_ready_database() {
    local_sqlite::database database = local_sqlite::open_local_content_database();
    if (!database.valid()) {
        return database;
    }
    if (!ensure_schema(database.get())) {
        return database;
    }
    return database;
}

std::optional<local_content_binding::song_binding> read_song(sqlite3_stmt* statement) {
    return local_content_binding::song_binding{
        .server_url = column_text(statement, 0),
        .local_song_id = column_text(statement, 1),
        .remote_song_id = column_text(statement, 2),
        .origin = origin_from_int(sqlite3_column_int(statement, 3)),
        .can_edit = column_optional_bool(statement, 4),
        .lifecycle_status = column_text(statement, 5),
        .review_status = column_text(statement, 6),
    };
}

std::optional<local_content_binding::chart_binding> read_chart(sqlite3_stmt* statement) {
    return local_content_binding::chart_binding{
        .server_url = column_text(statement, 0),
        .local_chart_id = column_text(statement, 1),
        .remote_chart_id = column_text(statement, 2),
        .remote_song_id = column_text(statement, 3),
        .remote_chart_version = sqlite3_column_int(statement, 4),
        .origin = origin_from_int(sqlite3_column_int(statement, 5)),
        .can_edit = column_optional_bool(statement, 6),
        .lifecycle_status = column_text(statement, 7),
        .review_status = column_text(statement, 8),
    };
}

std::optional<local_content_binding::song_binding> find_song(sqlite3* database,
                                                                  const char* sql,
                                                                  const std::string& server_url,
                                                                  const std::string& id) {
    statement query(database, sql);
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, id);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return read_song(query.get());
}

std::optional<local_content_binding::chart_binding> find_chart(sqlite3* database,
                                                                    const char* sql,
                                                                    const std::string& server_url,
                                                                    const std::string& id) {
    statement query(database, sql);
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, id);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return read_chart(query.get());
}

}  // namespace

local_content_binding::store load_mappings() {
    local_content_binding::store mappings;
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return mappings;
    }

    statement songs(database.get(),
                    "SELECT server_url, local_song_id, remote_song_id, origin, can_edit, "
                    "lifecycle_status, review_status "
                    "FROM song_bindings "
                    "ORDER BY server_url, local_song_id;");
    if (songs.valid()) {
        while (sqlite3_step(songs.get()) == SQLITE_ROW) {
            mappings.songs.push_back(*read_song(songs.get()));
        }
    }

    statement charts(database.get(),
                     "SELECT server_url, local_chart_id, remote_chart_id, remote_song_id, "
                     "remote_chart_version, origin, can_edit, lifecycle_status, review_status "
                     "FROM chart_bindings ORDER BY server_url, local_chart_id;");
    if (charts.valid()) {
        while (sqlite3_step(charts.get()) == SQLITE_ROW) {
            mappings.charts.push_back(*read_chart(charts.get()));
        }
    }

    return mappings;
}

std::optional<local_content_binding::song_binding> find_song_by_local(const std::string& server_url,
                                                                           const std::string& local_song_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_song(database.get(),
                         "SELECT server_url, local_song_id, remote_song_id, origin, can_edit, "
                         "lifecycle_status, review_status "
                         "FROM song_bindings "
                         "WHERE server_url = ? AND local_song_id = ?;",
                         server_url,
                         local_song_id);
    }
    return std::nullopt;
}

std::optional<local_content_binding::song_binding> find_song_by_remote(const std::string& server_url,
                                                                            const std::string& remote_song_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_song(database.get(),
                         "SELECT server_url, local_song_id, remote_song_id, origin, can_edit, "
                         "lifecycle_status, review_status "
                         "FROM song_bindings "
                         "WHERE server_url = ? AND remote_song_id = ?;",
                         server_url,
                         remote_song_id);
    }
    return std::nullopt;
}

std::optional<local_content_binding::chart_binding> find_chart_by_local(const std::string& server_url,
                                                                             const std::string& local_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_chart(database.get(),
                          "SELECT server_url, local_chart_id, remote_chart_id, remote_song_id, "
                          "remote_chart_version, origin, can_edit, lifecycle_status, review_status "
                          "FROM chart_bindings WHERE server_url = ? AND local_chart_id = ?;",
                          server_url,
                          local_chart_id);
    }
    return std::nullopt;
}

std::optional<local_content_binding::chart_binding> find_chart_by_remote(const std::string& server_url,
                                                                              const std::string& remote_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_chart(database.get(),
                          "SELECT server_url, local_chart_id, remote_chart_id, remote_song_id, "
                          "remote_chart_version, origin, can_edit, lifecycle_status, review_status "
                          "FROM chart_bindings WHERE server_url = ? AND remote_chart_id = ?;",
                          server_url,
                          remote_chart_id);
    }
    return std::nullopt;
}

void put_song(const local_content_binding::song_binding& binding) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        put_song(database.get(), binding);
        invalidate_prune_cache();
    }
}

void put_chart(const local_content_binding::chart_binding& binding) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        put_chart(database.get(), binding);
        invalidate_prune_cache();
    }
}

void remove_song(const std::string& server_url, const std::string& local_song_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }
    statement query(database.get(), "DELETE FROM song_bindings WHERE server_url = ? AND local_song_id = ?;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, local_song_id);
    if (step_done(query.get())) {
        invalidate_prune_cache();
    }
}

void remove_chart(const std::string& server_url, const std::string& local_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }
    statement query(database.get(), "DELETE FROM chart_bindings WHERE server_url = ? AND local_chart_id = ?;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, local_chart_id);
    if (step_done(query.get())) {
        invalidate_prune_cache();
    }
}

void remove_song_bindings(const std::string& local_song_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }
    statement query(database.get(), "DELETE FROM song_bindings WHERE local_song_id = ?;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, local_song_id);
    if (step_done(query.get())) {
        invalidate_prune_cache();
    }
}

void remove_chart_bindings(const std::string& local_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }
    statement query(database.get(), "DELETE FROM chart_bindings WHERE local_chart_id = ?;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, local_chart_id);
    if (step_done(query.get())) {
        invalidate_prune_cache();
    }
}

void remove_chart_bindings_for_remote_song(const std::string& server_url, const std::string& remote_song_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid() || server_url.empty() || remote_song_id.empty()) {
        return;
    }
    statement query(database.get(), "DELETE FROM chart_bindings WHERE server_url = ? AND remote_song_id = ?;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, server_url);
    bind_text(query.get(), 2, remote_song_id);
    if (step_done(query.get())) {
        invalidate_prune_cache();
    }
}

void prune_orphaned_bindings() {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        prune_orphaned_bindings(database.get());
    }
}

}  // namespace local_content_database
