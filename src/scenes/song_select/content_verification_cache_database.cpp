#include "song_select/content_verification_cache_database.h"

#include "local_sqlite.h"
#include "sqlite3.h"

namespace song_select::content_verification_cache_database {
namespace {

using local_sqlite::bind_text;
using local_sqlite::column_text;
using local_sqlite::exec;
using local_sqlite::statement;

std::string status_label(content_status status) {
    switch (status) {
    case content_status::official:
        return "official";
    case content_status::community:
        return "community";
    case content_status::update:
        return "update";
    case content_status::modified:
        return "modified";
    case content_status::checking:
        return "checking";
    case content_status::local:
    default:
        return "local";
    }
}

content_status parse_status(const std::string& value) {
    if (value == "official") {
        return content_status::official;
    }
    if (value == "community") {
        return content_status::community;
    }
    if (value == "update") {
        return content_status::update;
    }
    if (value == "modified") {
        return content_status::modified;
    }
    if (value == "checking") {
        return content_status::checking;
    }
    return content_status::local;
}

std::string cache_key(std::string_view server_url, std::string_view chart_id) {
    return std::string(server_url) + "\n" + std::string(chart_id);
}

bool ensure_schema(sqlite3* database) {
    return local_sqlite::ensure_common_schema(database) &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS content_verification_cache ("
                "server_url TEXT NOT NULL,"
                "chart_id TEXT NOT NULL,"
                "song_id TEXT NOT NULL,"
                "status TEXT NOT NULL,"
                "content_source TEXT NOT NULL,"
                "file_signature TEXT NOT NULL,"
                "local_song_json_sha256 TEXT NOT NULL,"
                "local_song_json_fingerprint TEXT NOT NULL,"
                "local_audio_sha256 TEXT NOT NULL,"
                "local_jacket_sha256 TEXT NOT NULL,"
                "local_chart_sha256 TEXT NOT NULL,"
                "local_chart_fingerprint TEXT NOT NULL,"
                "server_song_json_sha256 TEXT NOT NULL,"
                "server_song_json_fingerprint TEXT NOT NULL,"
                "server_audio_sha256 TEXT NOT NULL,"
                "server_jacket_sha256 TEXT NOT NULL,"
                "server_chart_sha256 TEXT NOT NULL,"
                "server_chart_fingerprint TEXT NOT NULL,"
                "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                "PRIMARY KEY (server_url, chart_id)"
                ");");
}

local_sqlite::database open_ready_database() {
    local_sqlite::database database = local_sqlite::open_local_content_database();
    if (database.valid()) {
        ensure_schema(database.get());
    }
    return database;
}

void bind_hashes(sqlite3_stmt* query, int start_index, const content_hashes& hashes) {
    bind_text(query, start_index, hashes.song_json_sha256);
    bind_text(query, start_index + 1, hashes.song_json_fingerprint);
    bind_text(query, start_index + 2, hashes.audio_sha256);
    bind_text(query, start_index + 3, hashes.jacket_sha256);
    bind_text(query, start_index + 4, hashes.chart_sha256);
    bind_text(query, start_index + 5, hashes.chart_fingerprint);
}

record read_record(sqlite3_stmt* query) {
    record item;
    item.server_url = column_text(query, 0);
    item.chart_id = column_text(query, 1);
    item.song_id = column_text(query, 2);
    item.status = parse_status(column_text(query, 3));
    item.content_source = column_text(query, 4);
    item.file_signature = column_text(query, 5);
    item.local_hashes = {
        .song_json_sha256 = column_text(query, 6),
        .song_json_fingerprint = column_text(query, 7),
        .audio_sha256 = column_text(query, 8),
        .jacket_sha256 = column_text(query, 9),
        .chart_sha256 = column_text(query, 10),
        .chart_fingerprint = column_text(query, 11),
    };
    item.server_hashes = {
        .song_json_sha256 = column_text(query, 12),
        .song_json_fingerprint = column_text(query, 13),
        .audio_sha256 = column_text(query, 14),
        .jacket_sha256 = column_text(query, 15),
        .chart_sha256 = column_text(query, 16),
        .chart_fingerprint = column_text(query, 17),
    };
    return item;
}

}  // namespace

cache load() {
    cache records;
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return records;
    }

    statement query(database.get(),
                    "SELECT server_url, chart_id, song_id, status, content_source, file_signature, "
                    "local_song_json_sha256, local_song_json_fingerprint, local_audio_sha256, "
                    "local_jacket_sha256, local_chart_sha256, local_chart_fingerprint, "
                    "server_song_json_sha256, server_song_json_fingerprint, server_audio_sha256, "
                    "server_jacket_sha256, server_chart_sha256, server_chart_fingerprint "
                    "FROM content_verification_cache;");
    if (!query.valid()) {
        return records;
    }

    while (sqlite3_step(query.get()) == SQLITE_ROW) {
        record item = read_record(query.get());
        if (!item.server_url.empty() && !item.chart_id.empty()) {
            records[cache_key(item.server_url, item.chart_id)] = std::move(item);
        }
    }
    return records;
}

void save(const cache& records) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }

    local_sqlite::transaction tx(database.get());
    if (!tx.active()) {
        return;
    }
    exec(database.get(), "DELETE FROM content_verification_cache;");

    statement query(database.get(),
                    "INSERT INTO content_verification_cache("
                    "server_url, chart_id, song_id, status, content_source, file_signature, "
                    "local_song_json_sha256, local_song_json_fingerprint, local_audio_sha256, "
                    "local_jacket_sha256, local_chart_sha256, local_chart_fingerprint, "
                    "server_song_json_sha256, server_song_json_fingerprint, server_audio_sha256, "
                    "server_jacket_sha256, server_chart_sha256, server_chart_fingerprint, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));");
    if (!query.valid()) {
        return;
    }

    for (const auto& [_, item] : records) {
        sqlite3_reset(query.get());
        sqlite3_clear_bindings(query.get());
        bind_text(query.get(), 1, item.server_url);
        bind_text(query.get(), 2, item.chart_id);
        bind_text(query.get(), 3, item.song_id);
        bind_text(query.get(), 4, status_label(item.status));
        bind_text(query.get(), 5, item.content_source);
        bind_text(query.get(), 6, item.file_signature);
        bind_hashes(query.get(), 7, item.local_hashes);
        bind_hashes(query.get(), 13, item.server_hashes);
        sqlite3_step(query.get());
    }
    tx.commit();
}

}  // namespace song_select::content_verification_cache_database
