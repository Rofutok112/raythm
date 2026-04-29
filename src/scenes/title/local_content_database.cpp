#include "title/local_content_database.h"

#include <ctime>
#include <cstdint>
#include <optional>
#include <string>

#include "local_sqlite.h"
#include "sqlite3.h"

namespace local_content_database {
namespace {

int origin_to_int(title_upload_mapping::mapping_origin origin) {
    switch (origin) {
    case title_upload_mapping::mapping_origin::downloaded:
        return 1;
    case title_upload_mapping::mapping_origin::linked:
        return 2;
    case title_upload_mapping::mapping_origin::owned_upload:
    default:
        return 0;
    }
}

title_upload_mapping::mapping_origin origin_from_int(int value) {
    switch (value) {
    case 1:
        return title_upload_mapping::mapping_origin::downloaded;
    case 2:
        return title_upload_mapping::mapping_origin::linked;
    case 0:
    default:
        return title_upload_mapping::mapping_origin::owned_upload;
    }
}

title_upload_mapping::mapping_origin merge_origin(title_upload_mapping::mapping_origin current,
                                                  title_upload_mapping::mapping_origin incoming) {
    if (current == title_upload_mapping::mapping_origin::owned_upload) {
        return current;
    }
    if (current == title_upload_mapping::mapping_origin::downloaded &&
        incoming == title_upload_mapping::mapping_origin::linked) {
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

int count_rows(sqlite3* database, const char* table_name) {
    const std::string sql = std::string("SELECT COUNT(*) FROM ") + table_name + ";";
    statement query(database, sql.c_str());
    if (!query.valid() || sqlite3_step(query.get()) != SQLITE_ROW) {
        return 0;
    }
    return sqlite3_column_int(query.get(), 0);
}

std::optional<title_upload_mapping::mapping_origin> current_song_origin(sqlite3* database,
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

std::optional<title_upload_mapping::mapping_origin> current_chart_origin(sqlite3* database,
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

void put_song(sqlite3* database, const title_upload_mapping::song_mapping_entry& binding) {
    if (binding.server_url.empty() || binding.local_song_id.empty() || binding.remote_song_id.empty()) {
        return;
    }

    const std::optional<title_upload_mapping::mapping_origin> current_origin =
        current_song_origin(database, binding.server_url, binding.local_song_id);
    const title_upload_mapping::mapping_origin origin = current_origin.has_value()
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

void put_chart(sqlite3* database, const title_upload_mapping::chart_mapping_entry& binding) {
    if (binding.server_url.empty() || binding.local_chart_id.empty() || binding.local_song_id.empty() ||
        binding.remote_chart_id.empty() || binding.remote_song_id.empty()) {
        return;
    }

    const std::optional<title_upload_mapping::mapping_origin> current_origin =
        current_chart_origin(database, binding.server_url, binding.local_chart_id);
    const title_upload_mapping::mapping_origin origin = current_origin.has_value()
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

bool imported_legacy(sqlite3* database) {
    return local_sqlite::metadata_value(database, "legacy_upload_mappings_imported").has_value();
}

void mark_legacy_imported(sqlite3* database) {
    local_sqlite::put_metadata(database, "legacy_upload_mappings_imported", "1");
}

void import_legacy_if_needed(sqlite3* database) {
    if (imported_legacy(database) ||
        count_rows(database, "song_bindings") > 0 ||
        count_rows(database, "chart_bindings") > 0) {
        return;
    }

    const title_upload_mapping::store legacy = title_upload_mapping::load();
    if (legacy.songs.empty() && legacy.charts.empty()) {
        mark_legacy_imported(database);
        return;
    }

    local_sqlite::transaction transaction(database);
    if (!transaction.active()) {
        return;
    }
    for (const title_upload_mapping::song_mapping_entry& song : legacy.songs) {
        put_song(database, song);
    }
    for (const title_upload_mapping::chart_mapping_entry& chart : legacy.charts) {
        put_chart(database, chart);
    }
    transaction.commit();
    mark_legacy_imported(database);
}

local_sqlite::database open_ready_database() {
    local_sqlite::database database = local_sqlite::open_local_content_database();
    if (!database.valid()) {
        return database;
    }
    if (!ensure_schema(database.get())) {
        return database;
    }
    import_legacy_if_needed(database.get());
    return database;
}

std::optional<title_upload_mapping::song_mapping_entry> read_song(sqlite3_stmt* statement) {
    return title_upload_mapping::song_mapping_entry{
        .server_url = column_text(statement, 0),
        .local_song_id = column_text(statement, 1),
        .remote_song_id = column_text(statement, 2),
        .origin = origin_from_int(sqlite3_column_int(statement, 3)),
    };
}

std::optional<title_upload_mapping::chart_mapping_entry> read_chart(sqlite3_stmt* statement) {
    return title_upload_mapping::chart_mapping_entry{
        .server_url = column_text(statement, 0),
        .local_chart_id = column_text(statement, 1),
        .local_song_id = column_text(statement, 2),
        .remote_chart_id = column_text(statement, 3),
        .remote_song_id = column_text(statement, 4),
        .origin = origin_from_int(sqlite3_column_int(statement, 5)),
    };
}

std::optional<title_upload_mapping::song_mapping_entry> find_song(sqlite3* database,
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

std::optional<title_upload_mapping::chart_mapping_entry> find_chart(sqlite3* database,
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

title_upload_mapping::store load_mappings() {
    title_upload_mapping::store mappings;
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return title_upload_mapping::load();
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

std::optional<title_upload_mapping::song_mapping_entry> find_song_by_local(const std::string& server_url,
                                                                           const std::string& local_song_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_song(database.get(),
                         "SELECT server_url, local_song_id, remote_song_id, origin FROM song_bindings "
                         "WHERE server_url = ? AND local_song_id = ?;",
                         server_url,
                         local_song_id);
    }

    const title_upload_mapping::store mappings = title_upload_mapping::load();
    const std::optional<std::string> remote_id =
        title_upload_mapping::find_remote_song_id(mappings, server_url, local_song_id);
    const std::optional<title_upload_mapping::mapping_origin> origin =
        title_upload_mapping::find_song_origin(mappings, server_url, local_song_id);
    if (!remote_id.has_value()) {
        return std::nullopt;
    }
    return title_upload_mapping::song_mapping_entry{
        .server_url = server_url,
        .local_song_id = local_song_id,
        .remote_song_id = *remote_id,
        .origin = origin.value_or(title_upload_mapping::mapping_origin::owned_upload),
    };
}

std::optional<title_upload_mapping::song_mapping_entry> find_song_by_remote(const std::string& server_url,
                                                                            const std::string& remote_song_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_song(database.get(),
                         "SELECT server_url, local_song_id, remote_song_id, origin FROM song_bindings "
                         "WHERE server_url = ? AND remote_song_id = ?;",
                         server_url,
                         remote_song_id);
    }

    const title_upload_mapping::store mappings = title_upload_mapping::load();
    const std::optional<std::string> local_id =
        title_upload_mapping::find_local_song_id(mappings, server_url, remote_song_id);
    if (!local_id.has_value()) {
        return std::nullopt;
    }
    return find_song_by_local(server_url, *local_id);
}

std::optional<title_upload_mapping::chart_mapping_entry> find_chart_by_local(const std::string& server_url,
                                                                             const std::string& local_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_chart(database.get(),
                          "SELECT server_url, local_chart_id, local_song_id, remote_chart_id, remote_song_id, origin "
                          "FROM chart_bindings WHERE server_url = ? AND local_chart_id = ?;",
                          server_url,
                          local_chart_id);
    }

    const title_upload_mapping::store mappings = title_upload_mapping::load();
    const std::optional<std::string> remote_id =
        title_upload_mapping::find_remote_chart_id(mappings, server_url, local_chart_id);
    const std::optional<title_upload_mapping::mapping_origin> origin =
        title_upload_mapping::find_chart_origin(mappings, server_url, local_chart_id);
    if (!remote_id.has_value()) {
        return std::nullopt;
    }
    for (const title_upload_mapping::chart_mapping_entry& chart : mappings.charts) {
        if (chart.server_url == server_url && chart.local_chart_id == local_chart_id) {
            return title_upload_mapping::chart_mapping_entry{
                .server_url = server_url,
                .local_chart_id = local_chart_id,
                .local_song_id = chart.local_song_id,
                .remote_chart_id = *remote_id,
                .remote_song_id = chart.remote_song_id,
                .origin = origin.value_or(title_upload_mapping::mapping_origin::owned_upload),
            };
        }
    }
    return std::nullopt;
}

std::optional<title_upload_mapping::chart_mapping_entry> find_chart_by_remote(const std::string& server_url,
                                                                              const std::string& remote_chart_id) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        return find_chart(database.get(),
                          "SELECT server_url, local_chart_id, local_song_id, remote_chart_id, remote_song_id, origin "
                          "FROM chart_bindings WHERE server_url = ? AND remote_chart_id = ?;",
                          server_url,
                          remote_chart_id);
    }

    const title_upload_mapping::store mappings = title_upload_mapping::load();
    const std::optional<std::string> local_id =
        title_upload_mapping::find_local_chart_id(mappings, server_url, remote_chart_id);
    if (!local_id.has_value()) {
        return std::nullopt;
    }
    return find_chart_by_local(server_url, *local_id);
}

void put_song(const title_upload_mapping::song_mapping_entry& binding) {
    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        put_song(database.get(), binding);
    }
}

void put_chart(const title_upload_mapping::chart_mapping_entry& binding) {
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
