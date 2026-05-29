#include "song_select/local_catalog_database.h"

#include <algorithm>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "local_catalog_signature.h"
#include "local_sqlite.h"
#include "network/json_helpers.h"
#include "sqlite3.h"

namespace song_select::local_catalog_database {
namespace {

using local_sqlite::bind_text;
using local_sqlite::column_text;
using local_sqlite::exec;
using local_sqlite::statement;

void ensure_optional_schema(sqlite3* database) {
    exec(database, "ALTER TABLE local_songs ADD COLUMN genre TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN duration_seconds REAL NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_songs ADD COLUMN source_status TEXT NOT NULL DEFAULT 'local';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN content_kind TEXT NOT NULL DEFAULT 'local';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN storage_policy TEXT NOT NULL DEFAULT 'plain_workspace';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN verification_state TEXT NOT NULL DEFAULT 'unchecked';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN online_server_url TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN online_song_id TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN online_source TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN song_offset INTEGER NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_songs ADD COLUMN song_has_offset INTEGER NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_songs ADD COLUMN timing_events TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_songs ADD COLUMN managed_manifest TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN min_bpm REAL NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_charts ADD COLUMN max_bpm REAL NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_charts ADD COLUMN source_status TEXT NOT NULL DEFAULT 'local';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN content_kind TEXT NOT NULL DEFAULT 'local';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN storage_policy TEXT NOT NULL DEFAULT 'plain_workspace';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN verification_state TEXT NOT NULL DEFAULT 'unchecked';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN online_server_url TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN online_song_id TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN online_chart_id TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN online_source TEXT NOT NULL DEFAULT '';");
    exec(database, "ALTER TABLE local_charts ADD COLUMN online_chart_version INTEGER NOT NULL DEFAULT 0;");
    exec(database, "ALTER TABLE local_charts ADD COLUMN managed_manifest TEXT NOT NULL DEFAULT '';");
}

bool ensure_schema(sqlite3* database) {
    const bool ready =
        local_sqlite::ensure_common_schema(database) &&
        exec(database,
             "CREATE TABLE IF NOT EXISTS local_songs ("
             "song_id TEXT PRIMARY KEY,"
             "title TEXT NOT NULL,"
             "artist TEXT NOT NULL,"
             "genre TEXT NOT NULL DEFAULT '',"
             "directory TEXT NOT NULL,"
             "audio_file TEXT NOT NULL,"
             "jacket_file TEXT NOT NULL,"
             "base_bpm REAL NOT NULL,"
             "duration_seconds REAL NOT NULL DEFAULT 0,"
             "preview_start_ms INTEGER NOT NULL,"
             "song_version INTEGER NOT NULL,"
             "status TEXT NOT NULL,"
             "source_status TEXT NOT NULL DEFAULT 'local',"
             "content_kind TEXT NOT NULL DEFAULT 'local',"
             "storage_policy TEXT NOT NULL DEFAULT 'plain_workspace',"
             "verification_state TEXT NOT NULL DEFAULT 'unchecked',"
             "online_server_url TEXT NOT NULL DEFAULT '',"
             "online_song_id TEXT NOT NULL DEFAULT '',"
             "online_source TEXT NOT NULL DEFAULT '',"
             "song_offset INTEGER NOT NULL DEFAULT 0,"
             "song_has_offset INTEGER NOT NULL DEFAULT 0,"
             "timing_events TEXT NOT NULL DEFAULT '',"
             "managed_manifest TEXT NOT NULL DEFAULT '',"
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
             "source_status TEXT NOT NULL DEFAULT 'local',"
             "content_kind TEXT NOT NULL DEFAULT 'local',"
             "storage_policy TEXT NOT NULL DEFAULT 'plain_workspace',"
             "verification_state TEXT NOT NULL DEFAULT 'unchecked',"
             "online_server_url TEXT NOT NULL DEFAULT '',"
             "online_song_id TEXT NOT NULL DEFAULT '',"
             "online_chart_id TEXT NOT NULL DEFAULT '',"
             "online_source TEXT NOT NULL DEFAULT '',"
             "online_chart_version INTEGER NOT NULL DEFAULT 0,"
             "managed_manifest TEXT NOT NULL DEFAULT '',"
             "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
             ");");
    if (ready) {
        ensure_optional_schema(database);
    }
    return ready;
}

local_sqlite::database open_ready_database() {
    local_sqlite::database database = local_sqlite::open_local_catalog_cache_database();
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

std::string content_kind_label(content_kind kind) {
    switch (kind) {
    case content_kind::official:
        return "official";
    case content_kind::community:
        return "community";
    case content_kind::local:
    default:
        return "local";
    }
}

std::string storage_policy_label(storage_policy policy) {
    return policy == storage_policy::managed_package ? "managed_package" : "plain_workspace";
}

std::string verification_state_label(verification_state state) {
    switch (state) {
    case verification_state::matched:
        return "matched";
    case verification_state::modified:
        return "modified";
    case verification_state::unavailable:
        return "unavailable";
    case verification_state::checking:
        return "checking";
    case verification_state::unchecked:
    default:
        return "unchecked";
    }
}

content_status parse_status_label(const std::string& value) {
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

content_kind parse_content_kind_label(const std::string& value) {
    if (value == "official") {
        return content_kind::official;
    }
    if (value == "community") {
        return content_kind::community;
    }
    return content_kind::local;
}

storage_policy parse_storage_policy_label(const std::string& value) {
    return value == "managed_package" ? storage_policy::managed_package
                                      : storage_policy::plain_workspace;
}

verification_state parse_verification_state_label(const std::string& value) {
    if (value == "matched") {
        return verification_state::matched;
    }
    if (value == "modified") {
        return verification_state::modified;
    }
    if (value == "unavailable") {
        return verification_state::unavailable;
    }
    if (value == "checking") {
        return verification_state::checking;
    }
    return verification_state::unchecked;
}

std::string serialize_managed_song_manifest(
    const std::optional<managed_song_manifest_metadata>& metadata) {
    if (!metadata.has_value()) {
        return {};
    }

    std::ostringstream output;
    output << "{"
           << "\"packageId\":\"" << network::json::escape_string(metadata->package_id) << "\","
           << "\"songJsonHash\":\"" << network::json::escape_string(metadata->song_json_hash) << "\","
           << "\"songJsonFingerprint\":\"" << network::json::escape_string(metadata->song_json_fingerprint) << "\","
           << "\"audioHash\":\"" << network::json::escape_string(metadata->audio_hash) << "\","
           << "\"jacketHash\":\"" << network::json::escape_string(metadata->jacket_hash) << "\","
           << "\"remoteSongJsonHash\":\"" << network::json::escape_string(metadata->remote_song_json_hash) << "\","
           << "\"remoteSongJsonFingerprint\":\""
           << network::json::escape_string(metadata->remote_song_json_fingerprint) << "\","
           << "\"remoteAudioHash\":\"" << network::json::escape_string(metadata->remote_audio_hash) << "\","
           << "\"remoteJacketHash\":\"" << network::json::escape_string(metadata->remote_jacket_hash) << "\","
           << "\"createdAt\":\"" << network::json::escape_string(metadata->created_at) << "\","
           << "\"updatedAt\":\"" << network::json::escape_string(metadata->updated_at) << "\""
           << "}";
    return output.str();
}

std::optional<managed_song_manifest_metadata> parse_managed_song_manifest(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    managed_song_manifest_metadata metadata;
    metadata.package_id = network::json::extract_string(value, "packageId").value_or("");
    metadata.song_json_hash = network::json::extract_string(value, "songJsonHash").value_or("");
    metadata.song_json_fingerprint = network::json::extract_string(value, "songJsonFingerprint").value_or("");
    metadata.audio_hash = network::json::extract_string(value, "audioHash").value_or("");
    metadata.jacket_hash = network::json::extract_string(value, "jacketHash").value_or("");
    metadata.remote_song_json_hash = network::json::extract_string(value, "remoteSongJsonHash").value_or("");
    metadata.remote_song_json_fingerprint =
        network::json::extract_string(value, "remoteSongJsonFingerprint").value_or("");
    metadata.remote_audio_hash = network::json::extract_string(value, "remoteAudioHash").value_or("");
    metadata.remote_jacket_hash = network::json::extract_string(value, "remoteJacketHash").value_or("");
    metadata.created_at = network::json::extract_string(value, "createdAt").value_or("");
    metadata.updated_at = network::json::extract_string(value, "updatedAt").value_or("");
    return metadata;
}

std::string serialize_managed_chart_manifest(
    const std::optional<managed_chart_manifest_metadata>& metadata) {
    if (!metadata.has_value()) {
        return {};
    }

    std::ostringstream output;
    output << "{"
           << "\"chartHash\":\"" << network::json::escape_string(metadata->chart_hash) << "\","
           << "\"chartFingerprint\":\"" << network::json::escape_string(metadata->chart_fingerprint) << "\","
           << "\"remoteChartHash\":\"" << network::json::escape_string(metadata->remote_chart_hash) << "\","
           << "\"remoteChartFingerprint\":\""
           << network::json::escape_string(metadata->remote_chart_fingerprint) << "\","
           << "\"revisionId\":\"" << network::json::escape_string(metadata->revision_id) << "\""
           << "}";
    return output.str();
}

std::optional<managed_chart_manifest_metadata> parse_managed_chart_manifest(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    managed_chart_manifest_metadata metadata;
    metadata.chart_hash = network::json::extract_string(value, "chartHash").value_or("");
    metadata.chart_fingerprint = network::json::extract_string(value, "chartFingerprint").value_or("");
    metadata.remote_chart_hash = network::json::extract_string(value, "remoteChartHash").value_or("");
    metadata.remote_chart_fingerprint =
        network::json::extract_string(value, "remoteChartFingerprint").value_or("");
    metadata.revision_id = network::json::extract_string(value, "revisionId").value_or("");
    return metadata;
}

std::string timing_type_label(timing_event_type type) {
    switch (type) {
    case timing_event_type::meter:
        return "meter";
    case timing_event_type::bpm:
    default:
        return "bpm";
    }
}

std::optional<timing_event_type> parse_timing_type(const std::string& value) {
    if (value == "bpm") {
        return timing_event_type::bpm;
    }
    if (value == "meter") {
        return timing_event_type::meter;
    }
    return std::nullopt;
}

std::string serialize_timing_events(const std::vector<timing_event>& events) {
    if (events.empty()) {
        return {};
    }

    std::ostringstream output;
    output << "[";
    for (size_t index = 0; index < events.size(); ++index) {
        const timing_event& event = events[index];
        if (index > 0) {
            output << ",";
        }
        output << "{\"type\":\"" << timing_type_label(event.type) << "\",\"tick\":" << event.tick;
        if (event.type == timing_event_type::bpm) {
            output << ",\"bpm\":" << event.bpm;
        } else {
            output << ",\"numerator\":" << event.numerator << ",\"denominator\":" << event.denominator;
        }
        output << "}";
    }
    output << "]";
    return output.str();
}

std::vector<timing_event> parse_timing_events(const std::string& value) {
    std::vector<timing_event> events;
    if (value.empty()) {
        return events;
    }

    const std::vector<std::string> objects = network::json::extract_objects_from_array(value);
    events.reserve(objects.size());
    for (const std::string& object : objects) {
        const std::optional<std::string> type_token = network::json::extract_string(object, "type");
        const std::optional<int> tick = network::json::extract_int(object, "tick");
        if (!type_token.has_value() || !tick.has_value() || *tick < 0) {
            continue;
        }

        const std::optional<timing_event_type> type = parse_timing_type(*type_token);
        if (!type.has_value()) {
            continue;
        }

        timing_event event;
        event.type = *type;
        event.tick = *tick;
        if (*type == timing_event_type::bpm) {
            const std::optional<float> bpm = network::json::extract_float(object, "bpm");
            if (!bpm.has_value() || *bpm <= 0.0f) {
                continue;
            }
            event.bpm = *bpm;
        } else {
            const std::optional<int> numerator = network::json::extract_int(object, "numerator");
            const std::optional<int> denominator = network::json::extract_int(object, "denominator");
            if (!numerator.has_value() || !denominator.has_value() || *numerator <= 0 || *denominator <= 0) {
                continue;
            }
            event.numerator = *numerator;
            event.denominator = *denominator;
        }
        events.push_back(event);
    }
    return events;
}

void put_song(sqlite3* database, const song_entry& song) {
    statement query(database,
                    "INSERT INTO local_songs(song_id, title, artist, genre, directory, audio_file, jacket_file, "
                    "base_bpm, duration_seconds, preview_start_ms, song_version, status, source_status, "
                    "content_kind, storage_policy, verification_state, "
                    "online_server_url, online_song_id, online_source, song_offset, song_has_offset, "
                    "timing_events, managed_manifest, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now')) "
                    "ON CONFLICT(song_id) DO UPDATE SET "
                    "title = excluded.title,"
                    "artist = excluded.artist,"
                    "genre = excluded.genre,"
                    "directory = excluded.directory,"
                    "audio_file = excluded.audio_file,"
                    "jacket_file = excluded.jacket_file,"
                    "base_bpm = excluded.base_bpm,"
                    "duration_seconds = excluded.duration_seconds,"
                    "preview_start_ms = excluded.preview_start_ms,"
                    "song_version = excluded.song_version,"
                    "status = excluded.status,"
                    "source_status = excluded.source_status,"
                    "content_kind = excluded.content_kind,"
                    "storage_policy = excluded.storage_policy,"
                    "verification_state = excluded.verification_state,"
                    "online_server_url = excluded.online_server_url,"
                    "online_song_id = excluded.online_song_id,"
                    "online_source = excluded.online_source,"
                    "song_offset = excluded.song_offset,"
                    "song_has_offset = excluded.song_has_offset,"
                    "timing_events = excluded.timing_events,"
                    "managed_manifest = excluded.managed_manifest,"
                    "updated_at = excluded.updated_at;");
    if (!query.valid() || song.song.meta.song_id.empty()) {
        return;
    }

    bind_text(query.get(), 1, song.song.meta.song_id);
    bind_text(query.get(), 2, song.song.meta.title);
    bind_text(query.get(), 3, song.song.meta.artist);
    bind_text(query.get(), 4, song.song.meta.genre);
    bind_text(query.get(), 5, song.song.directory);
    bind_text(query.get(), 6, song.song.meta.audio_file);
    bind_text(query.get(), 7, song.song.meta.jacket_file);
    sqlite3_bind_double(query.get(), 8, song.song.meta.base_bpm);
    sqlite3_bind_double(query.get(), 9, song.song.meta.duration_seconds);
    sqlite3_bind_int(query.get(), 10, song.song.meta.preview_start_ms);
    sqlite3_bind_int(query.get(), 11, song.song.meta.song_version);
    bind_text(query.get(), 12, status_label(song.status));
    bind_text(query.get(), 13, status_label(song.source_status));
    bind_text(query.get(), 14, content_kind_label(song.kind));
    bind_text(query.get(), 15, storage_policy_label(song.storage));
    bind_text(query.get(), 16, verification_state_label(song.verification));
    bind_text(query.get(), 17, song.online_identity.has_value() ? song.online_identity->server_url : "");
    bind_text(query.get(), 18, song.online_identity.has_value() ? song.online_identity->remote_song_id : "");
    bind_text(query.get(), 19, song.online_identity.has_value()
        ? online_content::source_label(song.online_identity->content_source)
        : "");
    sqlite3_bind_int(query.get(), 20, song.song.meta.offset);
    sqlite3_bind_int(query.get(), 21, song.song.meta.has_offset ? 1 : 0);
    bind_text(query.get(), 22, serialize_timing_events(song.song.meta.timing_events));
    bind_text(query.get(), 23, serialize_managed_song_manifest(song.managed_manifest));
    sqlite3_step(query.get());
}

void put_chart(sqlite3* database, const chart_option& chart) {
    statement query(database,
                    "INSERT INTO local_charts(chart_id, song_id, path, difficulty, level, key_count, "
                    "chart_author, format_version, note_count, min_bpm, max_bpm, status, source_status, "
                    "content_kind, storage_policy, verification_state, "
                    "online_server_url, online_song_id, online_chart_id, online_source, "
                    "online_chart_version, managed_manifest, updated_at) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now')) "
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
                    "source_status = excluded.source_status,"
                    "content_kind = excluded.content_kind,"
                    "storage_policy = excluded.storage_policy,"
                    "verification_state = excluded.verification_state,"
                    "online_server_url = excluded.online_server_url,"
                    "online_song_id = excluded.online_song_id,"
                    "online_chart_id = excluded.online_chart_id,"
                    "online_source = excluded.online_source,"
                    "online_chart_version = excluded.online_chart_version,"
                    "managed_manifest = excluded.managed_manifest,"
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
    bind_text(query.get(), 13, status_label(chart.source_status));
    bind_text(query.get(), 14, content_kind_label(chart.kind));
    bind_text(query.get(), 15, storage_policy_label(chart.storage));
    bind_text(query.get(), 16, verification_state_label(chart.verification));
    bind_text(query.get(), 17, chart.online_identity.has_value() ? chart.online_identity->server_url : "");
    bind_text(query.get(), 18, chart.online_identity.has_value() ? chart.online_identity->remote_song_id : "");
    bind_text(query.get(), 19, chart.online_identity.has_value() ? chart.online_identity->remote_chart_id : "");
    bind_text(query.get(), 20, chart.online_identity.has_value()
        ? online_content::source_label(chart.online_identity->content_source)
        : "");
    sqlite3_bind_int(query.get(), 21, chart.online_identity.has_value()
        ? chart.online_identity->remote_chart_version
        : chart.meta.chart_version);
    bind_text(query.get(), 22, serialize_managed_chart_manifest(chart.managed_manifest));
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
        local_catalog_signature::current()) {
        return catalog;
    }
    if (local_sqlite::metadata_value(database.get(), "local_catalog.status_schema").value_or("") !=
        local_catalog_signature::kStatusSchema) {
        return catalog;
    }

    std::map<std::string, song_entry> by_song_id;
    statement songs(database.get(),
                    "SELECT song_id, title, artist, genre, directory, audio_file, jacket_file, base_bpm, "
                    "duration_seconds, preview_start_ms, song_version, song_offset, song_has_offset, timing_events, "
                    "status, source_status, content_kind, storage_policy, verification_state, "
                    "online_server_url, online_song_id, online_source, managed_manifest "
                    "FROM local_songs ORDER BY title, song_id;");
    if (!songs.valid()) {
        return catalog;
    }
    while (sqlite3_step(songs.get()) == SQLITE_ROW) {
        song_entry entry;
        entry.song.meta.song_id = column_text(songs.get(), 0);
        entry.song.meta.title = column_text(songs.get(), 1);
        entry.song.meta.artist = column_text(songs.get(), 2);
        entry.song.meta.genre = column_text(songs.get(), 3);
        entry.song.directory = column_text(songs.get(), 4);
        entry.song.meta.audio_file = column_text(songs.get(), 5);
        entry.song.meta.jacket_file = column_text(songs.get(), 6);
        entry.song.meta.base_bpm = static_cast<float>(sqlite3_column_double(songs.get(), 7));
        entry.song.meta.duration_seconds = static_cast<float>(sqlite3_column_double(songs.get(), 8));
        entry.song.meta.preview_start_ms = sqlite3_column_int(songs.get(), 9);
        entry.song.meta.preview_start_seconds = static_cast<float>(entry.song.meta.preview_start_ms) / 1000.0f;
        entry.song.meta.song_version = sqlite3_column_int(songs.get(), 10);
        entry.song.meta.offset = sqlite3_column_int(songs.get(), 11);
        entry.song.meta.has_offset = sqlite3_column_int(songs.get(), 12) != 0;
        entry.song.meta.timing_events = parse_timing_events(column_text(songs.get(), 13));
        entry.status = parse_status_label(column_text(songs.get(), 14));
        entry.source_status = parse_status_label(column_text(songs.get(), 15));
        entry.kind = parse_content_kind_label(column_text(songs.get(), 16));
        entry.storage = parse_storage_policy_label(column_text(songs.get(), 17));
        entry.verification = parse_verification_state_label(column_text(songs.get(), 18));
        const std::string online_server_url = column_text(songs.get(), 19);
        const std::string online_song_id = column_text(songs.get(), 20);
        const std::optional<online_content::source> online_source =
            online_content::source_from_string(column_text(songs.get(), 21));
        if (!online_server_url.empty() && !online_song_id.empty() && online_source.has_value()) {
            entry.online_identity = online_content::song_identity{
                .server_url = online_server_url,
                .remote_song_id = online_song_id,
                .content_source = *online_source,
            };
        }
        entry.managed_manifest = parse_managed_song_manifest(column_text(songs.get(), 22));
        by_song_id[entry.song.meta.song_id] = std::move(entry);
    }

    statement charts(database.get(),
                     "SELECT chart_id, song_id, path, difficulty, level, key_count, chart_author, "
                     "format_version, note_count, min_bpm, max_bpm, status, source_status, "
                     "content_kind, storage_policy, verification_state, online_server_url, "
                     "online_song_id, online_chart_id, online_source, online_chart_version, managed_manifest "
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
        chart.status = parse_status_label(column_text(charts.get(), 11));
        chart.source_status = parse_status_label(column_text(charts.get(), 12));
        chart.kind = parse_content_kind_label(column_text(charts.get(), 13));
        chart.storage = parse_storage_policy_label(column_text(charts.get(), 14));
        chart.verification = parse_verification_state_label(column_text(charts.get(), 15));
        const std::string online_server_url = column_text(charts.get(), 16);
        const std::string online_song_id = column_text(charts.get(), 17);
        const std::string online_chart_id = column_text(charts.get(), 18);
        const std::optional<online_content::source> online_source =
            online_content::source_from_string(column_text(charts.get(), 19));
        const int online_chart_version = sqlite3_column_int(charts.get(), 20);
        if (!online_server_url.empty() && !online_song_id.empty() &&
            !online_chart_id.empty() && online_source.has_value()) {
            chart.online_identity = online_content::chart_identity{
                .server_url = online_server_url,
                .remote_song_id = online_song_id,
                .remote_chart_id = online_chart_id,
                .content_source = *online_source,
                .remote_chart_version = online_chart_version,
            };
        }
        chart.managed_manifest = parse_managed_chart_manifest(column_text(charts.get(), 21));
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
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", local_catalog_signature::current());
    local_sqlite::put_metadata(database.get(), "local_catalog.status_schema", local_catalog_signature::kStatusSchema);
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
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", local_catalog_signature::current());
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
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", local_catalog_signature::current());
}

}  // namespace song_select::local_catalog_database
