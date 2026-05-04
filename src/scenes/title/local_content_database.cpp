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

bool ensure_schema(sqlite3* database) {
    return local_sqlite::ensure_common_schema(database) &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS song_bindings ("
                "server_url TEXT NOT NULL,"
                "local_song_id TEXT NOT NULL,"
                "remote_song_id TEXT NOT NULL,"
                "origin INTEGER NOT NULL,"
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
                "local_song_id TEXT NOT NULL,"
                "remote_chart_id TEXT NOT NULL,"
                "remote_song_id TEXT NOT NULL,"
                "origin INTEGER NOT NULL,"
                "updated_at INTEGER NOT NULL,"
                "PRIMARY KEY (server_url, local_chart_id)"
                ");") &&
           exec(database,
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_chart_bindings_remote "
                "ON chart_bindings(server_url, remote_chart_id);") &&
           exec(database,
                "INSERT INTO metadata(key, value) VALUES('schema_version', '1') "
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
                    "INSERT INTO song_bindings(server_url, local_song_id, remote_song_id, origin, updated_at) "
                    "VALUES(?, ?, ?, ?, ?) "
                    "ON CONFLICT(server_url, local_song_id) DO UPDATE SET "
                    "remote_song_id = excluded.remote_song_id,"
                    "origin = excluded.origin,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, binding.server_url);
    bind_text(query.get(), 2, binding.local_song_id);
    bind_text(query.get(), 3, binding.remote_song_id);
    sqlite3_bind_int(query.get(), 4, origin_to_int(origin));
    sqlite3_bind_int64(query.get(), 5, now_unix_seconds());
    step_done(query.get());
}

void put_chart(sqlite3* database, const local_content_binding::chart_binding& binding) {
    if (binding.server_url.empty() || binding.local_chart_id.empty() || binding.local_song_id.empty() ||
        binding.remote_chart_id.empty() || binding.remote_song_id.empty()) {
        return;
    }

    const std::optional<local_content_binding::origin> current_origin =
        current_chart_origin(database, binding.server_url, binding.local_chart_id);
    const local_content_binding::origin origin = current_origin.has_value()
        ? merge_origin(*current_origin, binding.origin)
        : binding.origin;

    statement query(database,
                    "INSERT INTO chart_bindings(server_url, local_chart_id, local_song_id, remote_chart_id, "
                    "remote_song_id, origin, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(server_url, local_chart_id) DO UPDATE SET "
                    "local_song_id = excluded.local_song_id,"
                    "remote_chart_id = excluded.remote_chart_id,"
                    "remote_song_id = excluded.remote_song_id,"
                    "origin = excluded.origin,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid()) {
        return;
    }
    bind_text(query.get(), 1, binding.server_url);
    bind_text(query.get(), 2, binding.local_chart_id);
    bind_text(query.get(), 3, binding.local_song_id);
    bind_text(query.get(), 4, binding.remote_chart_id);
    bind_text(query.get(), 5, binding.remote_song_id);
    sqlite3_bind_int(query.get(), 6, origin_to_int(origin));
    sqlite3_bind_int64(query.get(), 7, now_unix_seconds());
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
    };
}

std::optional<local_content_binding::chart_binding> read_chart(sqlite3_stmt* statement) {
    return local_content_binding::chart_binding{
        .server_url = column_text(statement, 0),
        .local_chart_id = column_text(statement, 1),
        .local_song_id = column_text(statement, 2),
        .remote_chart_id = column_text(statement, 3),
        .remote_song_id = column_text(statement, 4),
        .origin = origin_from_int(sqlite3_column_int(statement, 5)),
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
                    "SELECT server_url, local_song_id, remote_song_id, origin FROM song_bindings "
                    "ORDER BY server_url, local_song_id;");
    if (songs.valid()) {
        while (sqlite3_step(songs.get()) == SQLITE_ROW) {
            mappings.songs.push_back({
                .server_url = column_text(songs.get(), 0),
                .local_song_id = column_text(songs.get(), 1),
                .remote_song_id = column_text(songs.get(), 2),
                .origin = origin_from_int(sqlite3_column_int(songs.get(), 3)),
            });
        }
    }

    statement charts(database.get(),
                     "SELECT server_url, local_chart_id, local_song_id, remote_chart_id, remote_song_id, origin "
                     "FROM chart_bindings ORDER BY server_url, local_chart_id;");
    if (charts.valid()) {
        while (sqlite3_step(charts.get()) == SQLITE_ROW) {
            mappings.charts.push_back({
                .server_url = column_text(charts.get(), 0),
                .local_chart_id = column_text(charts.get(), 1),
                .local_song_id = column_text(charts.get(), 2),
                .remote_chart_id = column_text(charts.get(), 3),
                .remote_song_id = column_text(charts.get(), 4),
                .origin = origin_from_int(sqlite3_column_int(charts.get(), 5)),
            });
        }
    }

    return mappings;
}

std::optional<local_content_binding::song_binding> find_song_by_local(const std::string& server_url,
                                                                           const std::string& local_song_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_song(database.get(),
                         "SELECT server_url, local_song_id, remote_song_id, origin FROM song_bindings "
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
                         "SELECT server_url, local_song_id, remote_song_id, origin FROM song_bindings "
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
                          "SELECT server_url, local_chart_id, local_song_id, remote_chart_id, remote_song_id, origin "
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
                          "SELECT server_url, local_chart_id, local_song_id, remote_chart_id, remote_song_id, origin "
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

}  // namespace local_content_database
