#include "song_select/local_catalog_database.h"

#include <filesystem>
#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "app_paths.h"
#include "local_sqlite.h"
#include "sqlite3.h"

namespace song_select::local_catalog_database {
namespace {

using local_sqlite::bind_text;
using local_sqlite::column_text;
using local_sqlite::exec;
using local_sqlite::statement;

void ensure_optional_schema(sqlite3* database) {
    exec(database, "ALTER TABLE local_charts ADD COLUMN min_bpm REAL NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_charts ADD COLUMN max_bpm REAL NOT NULL DEFAULT 0;");
}

bool ensure_schema(sqlite3* database) {
    const bool ready =
        local_sqlite::ensure_common_schema(database) &&
        exec(database,
             "CREATE TABLE IF NOT EXISTS local_songs ("
             "song_id TEXT PRIMARY KEY,"
             "title TEXT NOT NULL,"
             "artist TEXT NOT NULL,"
             "directory TEXT NOT NULL,"
             "audio_file TEXT NOT NULL,"
             "jacket_file TEXT NOT NULL,"
             "base_bpm REAL NOT NULL,"
             "preview_start_ms INTEGER NOT NULL,"
             "song_version INTEGER NOT NULL,"
             "status TEXT NOT NULL,"
             "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
             ");") &&
        exec(database,
             "CREATE TABLE IF NOT EXISTS local_charts ("
             "chart_id TEXT PRIMARY KEY,"
             "song_id TEXT NOT NULL,"
             "path TEXT NOT NULL,"
             "difficulty TEXT NOT NULL,"
             "level REAL NOT NULL,"
             "key_count INTEGER NOT NULL,"
             "chart_author TEXT NOT NULL,"
             "format_version INTEGER NOT NULL,"
             "note_count INTEGER NOT NULL,"
             "min_bpm REAL NOT NULL DEFAULT 0,"
             "max_bpm REAL NOT NULL DEFAULT 0,"
             "status TEXT NOT NULL,"
             "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
             ");");
    if (ready) {
        ensure_optional_schema(database);
    }
    return ready;
}

std::string path_key(const std::filesystem::path& root, const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(path, root, ec);
    return ec ? path.string() : relative.generic_string();
}

void append_tree_signature(std::ostringstream& output, const std::filesystem::path& root, const char* label) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        output << label << ":missing\n";
        return;
    }

    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec)) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    output << label << ":";
    for (const std::filesystem::path& file : files) {
        const auto size = std::filesystem::file_size(file, ec);
        if (ec) {
            continue;
        }
        const auto write_time = std::filesystem::last_write_time(file, ec);
        if (ec) {
            continue;
        }
        output << path_key(root, file) << "," << size << "," << write_time.time_since_epoch().count() << ";";
    }
    output << "\n";
}

std::string current_catalog_signature() {
    std::ostringstream output;
    append_tree_signature(output, app_paths::songs_root(), "songs");
    append_tree_signature(output, app_paths::charts_root(), "charts");
    return output.str();
}

local_sqlite::database open_ready_database() {
    local_sqlite::database database = local_sqlite::open_local_content_database();
    if (database.valid()) {
        ensure_schema(database.get());
    }
    return database;
}

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

content_status parse_status(std::string value) {
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

void put_song(sqlite3* database, const song_entry& song) {
    statement query(database,
                    "INSERT INTO local_songs(song_id, title, artist, directory, audio_file, jacket_file, "
                    "base_bpm, preview_start_ms, song_version, status, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now')) "
                    "ON CONFLICT(song_id) DO UPDATE SET "
                    "title = excluded.title,"
                    "artist = excluded.artist,"
                    "directory = excluded.directory,"
                    "audio_file = excluded.audio_file,"
                    "jacket_file = excluded.jacket_file,"
                    "base_bpm = excluded.base_bpm,"
                    "preview_start_ms = excluded.preview_start_ms,"
                    "song_version = excluded.song_version,"
                    "status = excluded.status,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid() || song.song.meta.song_id.empty()) {
        return;
    }

    bind_text(query.get(), 1, song.song.meta.song_id);
    bind_text(query.get(), 2, song.song.meta.title);
    bind_text(query.get(), 3, song.song.meta.artist);
    bind_text(query.get(), 4, song.song.directory);
    bind_text(query.get(), 5, song.song.meta.audio_file);
    bind_text(query.get(), 6, song.song.meta.jacket_file);
    sqlite3_bind_double(query.get(), 7, song.song.meta.base_bpm);
    sqlite3_bind_int(query.get(), 8, song.song.meta.preview_start_ms);
    sqlite3_bind_int(query.get(), 9, song.song.meta.song_version);
    bind_text(query.get(), 10, status_label(song.status));
    sqlite3_step(query.get());
}

void put_chart(sqlite3* database, const chart_option& chart) {
    statement query(database,
                    "INSERT INTO local_charts(chart_id, song_id, path, difficulty, level, key_count, "
                    "chart_author, format_version, note_count, min_bpm, max_bpm, status, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now')) "
                    "ON CONFLICT(chart_id) DO UPDATE SET "
                    "song_id = excluded.song_id,"
                    "path = excluded.path,"
                    "difficulty = excluded.difficulty,"
                    "level = excluded.level,"
                    "key_count = excluded.key_count,"
                    "chart_author = excluded.chart_author,"
                    "format_version = excluded.format_version,"
                    "note_count = excluded.note_count,"
                    "min_bpm = excluded.min_bpm,"
                    "max_bpm = excluded.max_bpm,"
                    "status = excluded.status,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid() || chart.meta.chart_id.empty()) {
        return;
    }

    bind_text(query.get(), 1, chart.meta.chart_id);
    bind_text(query.get(), 2, chart.meta.song_id);
    bind_text(query.get(), 3, chart.path);
    bind_text(query.get(), 4, chart.meta.difficulty);
    sqlite3_bind_double(query.get(), 5, chart.meta.level);
    sqlite3_bind_int(query.get(), 6, chart.meta.key_count);
    bind_text(query.get(), 7, chart.meta.chart_author);
    sqlite3_bind_int(query.get(), 8, chart.meta.format_version);
    sqlite3_bind_int(query.get(), 9, chart.note_count);
    sqlite3_bind_double(query.get(), 10, chart.min_bpm);
    sqlite3_bind_double(query.get(), 11, chart.max_bpm);
    bind_text(query.get(), 12, status_label(chart.status));
    sqlite3_step(query.get());
}

}  // namespace

catalog_data load_cached_catalog() {
    catalog_data catalog;
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return catalog;
    }
    if (local_sqlite::metadata_value(database.get(), "local_catalog.signature").value_or("") !=
        current_catalog_signature()) {
        return catalog;
    }

    std::map<std::string, song_entry> by_song_id;
    statement songs(database.get(),
                    "SELECT song_id, title, artist, directory, audio_file, jacket_file, base_bpm, "
                    "preview_start_ms, song_version, status FROM local_songs ORDER BY title, song_id;");
    if (!songs.valid()) {
        return catalog;
    }
    while (sqlite3_step(songs.get()) == SQLITE_ROW) {
        song_entry entry;
        entry.song.meta.song_id = column_text(songs.get(), 0);
        entry.song.meta.title = column_text(songs.get(), 1);
        entry.song.meta.artist = column_text(songs.get(), 2);
        entry.song.directory = column_text(songs.get(), 3);
        entry.song.meta.audio_file = column_text(songs.get(), 4);
        entry.song.meta.jacket_file = column_text(songs.get(), 5);
        entry.song.meta.base_bpm = static_cast<float>(sqlite3_column_double(songs.get(), 6));
        entry.song.meta.preview_start_ms = sqlite3_column_int(songs.get(), 7);
        entry.song.meta.preview_start_seconds = static_cast<float>(entry.song.meta.preview_start_ms) / 1000.0f;
        entry.song.meta.song_version = sqlite3_column_int(songs.get(), 8);
        entry.status = parse_status(column_text(songs.get(), 9));
        by_song_id[entry.song.meta.song_id] = std::move(entry);
    }

    statement charts(database.get(),
                     "SELECT chart_id, song_id, path, difficulty, level, key_count, chart_author, "
                     "format_version, note_count, min_bpm, max_bpm, status "
                     "FROM local_charts ORDER BY song_id, level, difficulty;");
    if (!charts.valid()) {
        return catalog;
    }
    while (sqlite3_step(charts.get()) == SQLITE_ROW) {
        const std::string song_id = column_text(charts.get(), 1);
        auto song_it = by_song_id.find(song_id);
        if (song_it == by_song_id.end()) {
            continue;
        }

        chart_option chart;
        chart.meta.chart_id = column_text(charts.get(), 0);
        chart.meta.song_id = song_id;
        chart.path = column_text(charts.get(), 2);
        chart.meta.difficulty = column_text(charts.get(), 3);
        chart.meta.level = static_cast<float>(sqlite3_column_double(charts.get(), 4));
        chart.meta.key_count = sqlite3_column_int(charts.get(), 5);
        chart.meta.chart_author = column_text(charts.get(), 6);
        chart.meta.format_version = sqlite3_column_int(charts.get(), 7);
        chart.note_count = sqlite3_column_int(charts.get(), 8);
        chart.min_bpm = static_cast<float>(sqlite3_column_double(charts.get(), 9));
        chart.max_bpm = static_cast<float>(sqlite3_column_double(charts.get(), 10));
        chart.status = parse_status(column_text(charts.get(), 11));
        song_it->second.song.chart_paths.push_back(chart.path);
        song_it->second.charts.push_back(std::move(chart));
    }

    catalog.songs.reserve(by_song_id.size());
    for (auto& [song_id, song] : by_song_id) {
        (void)song_id;
        catalog.songs.push_back(std::move(song));
    }
    return catalog;
}

void replace_catalog(const std::vector<song_entry>& songs) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        return;
    }

    local_sqlite::transaction tx(database.get());
    if (!tx.active()) {
        return;
    }
    exec(database.get(), "DELETE FROM local_charts;");
    exec(database.get(), "DELETE FROM local_songs;");
    for (const song_entry& song : songs) {
        put_song(database.get(), song);
        for (const chart_option& chart : song.charts) {
            put_chart(database.get(), chart);
        }
    }
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", current_catalog_signature());
    tx.commit();
}

void remove_song(const std::string& song_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid() || song_id.empty()) {
        return;
    }

    statement charts(database.get(), "DELETE FROM local_charts WHERE song_id = ?;");
    if (charts.valid()) {
        bind_text(charts.get(), 1, song_id);
        sqlite3_step(charts.get());
    }

    statement songs(database.get(), "DELETE FROM local_songs WHERE song_id = ?;");
    if (songs.valid()) {
        bind_text(songs.get(), 1, song_id);
        sqlite3_step(songs.get());
    }
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", current_catalog_signature());
}

void remove_chart(const std::string& chart_id) {
    local_sqlite::database database = open_ready_database();
    if (!database.valid() || chart_id.empty()) {
        return;
    }

    statement charts(database.get(), "DELETE FROM local_charts WHERE chart_id = ?;");
    if (charts.valid()) {
        bind_text(charts.get(), 1, chart_id);
        sqlite3_step(charts.get());
    }
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", current_catalog_signature());
}

}  // namespace song_select::local_catalog_database
