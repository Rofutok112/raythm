#include "title/local_content_database.h"

#include <ctime>
#include <cstdint>
#include <optional>
#include <string>

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
                "CREATE UNIQUE INDEX idx_chart_bindings_remote "
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
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_song_bindings_remote "
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
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_chart_bindings_remote "
                "ON chart_bindings(server_url, remote_chart_id);") &&
           migrate_chart_bindings_without_local_song_id(database) &&
           ensure_chart_binding_version_column(database) &&
           ensure_binding_permission_columns(database, "song_bindings") &&
           ensure_binding_permission_columns(database, "chart_bindings") &&
           exec(database,
                "INSERT INTO metadata(key, value) VALUES('schema_version', '4') "
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
    }
}

void put_chart(const local_content_binding::chart_binding& binding) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        put_chart(database.get(), binding);
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
    step_done(query.get());
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
    step_done(query.get());
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
    step_done(query.get());
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
    step_done(query.get());
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
    step_done(query.get());
}

}  // namespace local_content_database
