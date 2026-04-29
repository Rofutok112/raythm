#include "chart_identity_store.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app_paths.h"
#include "local_sqlite.h"
#include "sqlite3.h"

namespace chart_identity {
namespace {

constexpr char kHeader[] = "# raythm chart identity index v1";

struct entry {
    std::string chart_id;
    std::string song_id;
};

std::vector<std::string> split_tab_fields(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

std::vector<entry> load_legacy_entries() {
    std::ifstream input(app_paths::chart_identity_index_path(), std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::vector<entry> entries;
    std::string line;
    while (std::getline(input, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> fields = split_tab_fields(line);
        if (fields.size() >= 2 && !fields[0].empty() && !fields[1].empty()) {
            entries.push_back({
                .chart_id = fields[0],
                .song_id = fields[1],
            });
        }
    }
    return entries;
}

void save_legacy_entries(const std::vector<entry>& entries) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::chart_identity_index_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << kHeader << '\n';
    for (const entry& item : entries) {
        output << item.chart_id << '\t' << item.song_id << '\n';
    }
}

using local_sqlite::bind_text;
using local_sqlite::column_text;
using local_sqlite::exec;
using local_sqlite::statement;

bool ensure_schema(sqlite3* database) {
    return local_sqlite::ensure_common_schema(database) &&
           exec(database,
                "CREATE TABLE IF NOT EXISTS chart_song_links ("
                "chart_id TEXT PRIMARY KEY,"
                "song_id TEXT NOT NULL,"
                "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                ");");
}

bool legacy_imported(sqlite3* database) {
    return local_sqlite::metadata_value(database, "legacy_chart_identity_imported").has_value();
}

void mark_legacy_imported(sqlite3* database) {
    local_sqlite::put_metadata(database, "legacy_chart_identity_imported", "1");
}

int count_links(sqlite3* database) {
    statement query(database, "SELECT COUNT(*) FROM chart_song_links;");
    if (!query.valid() || sqlite3_step(query.get()) != SQLITE_ROW) {
        return 0;
    }
    return sqlite3_column_int(query.get(), 0);
}

bool put_db(sqlite3* database, const std::string& chart_id, const std::string& song_id) {
    statement query(database,
                    "INSERT INTO chart_song_links(chart_id, song_id, updated_at) "
                    "VALUES(?, ?, strftime('%s','now')) "
                    "ON CONFLICT(chart_id) DO UPDATE SET "
                    "song_id = excluded.song_id,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid()) {
        return false;
    }
    bind_text(query.get(), 1, chart_id);
    bind_text(query.get(), 2, song_id);
    return sqlite3_step(query.get()) == SQLITE_DONE;
}

void import_legacy_if_needed(sqlite3* database) {
    if (legacy_imported(database) || count_links(database) > 0) {
        return;
    }

    const std::vector<entry> legacy = load_legacy_entries();
    if (legacy.empty()) {
        mark_legacy_imported(database);
        return;
    }

    local_sqlite::transaction transaction(database);
    if (!transaction.active()) {
        return;
    }
    for (const entry& item : legacy) {
        put_db(database, item.chart_id, item.song_id);
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

}  // namespace

std::optional<std::string> find_song_id(const std::string& chart_id) {
    if (chart_id.empty()) {
        return std::nullopt;
    }

    local_sqlite::database database = open_ready_database();
    if (!database.valid()) {
        for (const entry& item : load_legacy_entries()) {
            if (item.chart_id == chart_id) {
                return item.song_id;
            }
        }
        return std::nullopt;
    }

    statement query(database.get(), "SELECT song_id FROM chart_song_links WHERE chart_id = ?;");
    if (!query.valid()) {
        return std::nullopt;
    }
    bind_text(query.get(), 1, chart_id);
    if (sqlite3_step(query.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return column_text(query.get(), 0);
}

void put(const std::string& chart_id, const std::string& song_id) {
    if (chart_id.empty() || song_id.empty()) {
        return;
    }

    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        put_db(database.get(), chart_id, song_id);
        return;
    }

    std::vector<entry> entries = load_legacy_entries();
    for (entry& item : entries) {
        if (item.chart_id == chart_id) {
            item.song_id = song_id;
            save_legacy_entries(entries);
            return;
        }
    }

    entries.push_back({
        .chart_id = chart_id,
        .song_id = song_id,
    });
    save_legacy_entries(entries);
}

void remove(const std::string& chart_id) {
    if (chart_id.empty()) {
        return;
    }

    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        statement query(database.get(), "DELETE FROM chart_song_links WHERE chart_id = ?;");
        if (query.valid()) {
            bind_text(query.get(), 1, chart_id);
            sqlite3_step(query.get());
        }
        return;
    }

    std::vector<entry> entries = load_legacy_entries();
    std::erase_if(entries, [&](const entry& item) {
        return item.chart_id == chart_id;
    });
    save_legacy_entries(entries);
}

void remove_for_song(const std::string& song_id) {
    if (song_id.empty()) {
        return;
    }

    local_sqlite::database database = open_ready_database();
    if (database.valid()) {
        statement query(database.get(), "DELETE FROM chart_song_links WHERE song_id = ?;");
        if (query.valid()) {
            bind_text(query.get(), 1, song_id);
            sqlite3_step(query.get());
        }
        return;
    }

    std::vector<entry> entries = load_legacy_entries();
    std::erase_if(entries, [&](const entry& item) {
        return item.song_id == song_id;
    });
    save_legacy_entries(entries);
}

}  // namespace chart_identity
