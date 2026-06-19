#include "app_paths.h"
#include "local_sqlite.h"
#include "path_utils.h"
#include "song_select/local_catalog_cache_service.h"
#include "song_select/local_catalog_database.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "sqlite3.h"

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace fs = std::filesystem;

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

void assert_database_stored_managed_content() {
    local_sqlite::database database = local_sqlite::open_local_catalog_cache_database();
    assert(database.valid());

    local_sqlite::statement song(database.get(),
                                 "SELECT status, source_status, content_kind, storage_policy, verification_state, "
                                 "online_server_url, online_song_id, online_source "
                                 "FROM local_songs WHERE song_id = 'song-a';");
    assert(song.valid());
    assert(sqlite3_step(song.get()) == SQLITE_ROW);
    assert(local_sqlite::column_text(song.get(), 0) == "community");
    assert(local_sqlite::column_text(song.get(), 1) == "community");
    assert(local_sqlite::column_text(song.get(), 2) == "community");
    assert(local_sqlite::column_text(song.get(), 3) == "managed_package");
    assert(local_sqlite::column_text(song.get(), 4) == "matched");
    assert(local_sqlite::column_text(song.get(), 5) == "https://server.example");
    assert(local_sqlite::column_text(song.get(), 6) == "remote-song-a");
    assert(local_sqlite::column_text(song.get(), 7) == "community");

    local_sqlite::statement chart(database.get(),
                                  "SELECT status, source_status, content_kind, storage_policy, verification_state, "
                                  "online_server_url, online_song_id, online_chart_id, online_source, online_chart_version "
                                  "FROM local_charts WHERE chart_id = 'chart-a';");
    assert(chart.valid());
    assert(sqlite3_step(chart.get()) == SQLITE_ROW);
    assert(local_sqlite::column_text(chart.get(), 0) == "modified");
    assert(local_sqlite::column_text(chart.get(), 1) == "community");
    assert(local_sqlite::column_text(chart.get(), 2) == "community");
    assert(local_sqlite::column_text(chart.get(), 3) == "managed_package");
    assert(local_sqlite::column_text(chart.get(), 4) == "matched");
    assert(local_sqlite::column_text(chart.get(), 5) == "https://server.example");
    assert(local_sqlite::column_text(chart.get(), 6) == "remote-song-a");
    assert(local_sqlite::column_text(chart.get(), 7) == "remote-chart-a");
    assert(local_sqlite::column_text(chart.get(), 8) == "community");
    assert(sqlite3_column_int(chart.get(), 9) == 7);
}

song_select::chart_option make_chart(const char* chart_id, const char* song_id, const fs::path& path) {
    song_select::chart_option chart;
    chart.path = path.string();
    chart.meta.chart_id = chart_id;
    chart.meta.song_id = song_id;
    chart.meta.difficulty = "Hard";
    chart.meta.level = 7.5f;
    chart.meta.key_count = 4;
    chart.meta.chart_author = "tester";
    chart.meta.format_version = 2;
    chart.note_count = 123;
    chart.min_bpm = 140.0f;
    chart.max_bpm = 190.0f;
    chart.kind = content_kind::community;
    chart.storage = storage_policy::managed_package;
    chart.verification = verification_state::matched;
    chart.status = content_status::modified;
    chart.source_status = content_status::community;
    chart.source = content_source::community;
    chart.sync_state = content_sync_state::modified;
    chart.meta.extra.unlock = {
        .unlock_state = "locked",
        .locked = true,
        .can_download = true,
        .can_play = false,
        .lock_reason = "Clear KEY to play this chart.",
        .unlock_rule_count = 1,
    };
    chart.online_identity = online_content::chart_identity{
        .server_url = "https://server.example",
        .remote_song_id = "remote-song-a",
        .remote_chart_id = "remote-chart-a",
        .content_source = online_content::source::community,
        .remote_chart_version = 7,
    };
    chart.managed_manifest = song_select::managed_chart_manifest_metadata{
        .chart_hash = "local-chart-sha",
        .chart_fingerprint = "local-chart-fingerprint",
        .remote_chart_hash = "remote-chart-sha",
        .remote_chart_fingerprint = "remote-chart-fingerprint",
        .revision_id = "chart-rev-a",
    };
    chart.remote_links.push_back(online_content::chart_identity{
        .server_url = "https://server.example",
        .remote_song_id = "remote-song-a",
        .remote_chart_id = "remote-chart-a",
        .content_source = online_content::source::community,
        .remote_chart_version = 7,
    });
    return chart;
}

song_select::song_entry make_song(const fs::path& song_dir, const fs::path& chart_path) {
    song_select::song_entry song;
    song.song.meta.song_id = "song-a";
    song.song.meta.title = "Alpha";
    song.song.meta.artist = "Artist";
    song.song.meta.genre = "Fusion";
    song.song.meta.duration_seconds = 95.0f;
    song.song.meta.audio_file = "audio.ogg";
    song.song.meta.jacket_file = "jacket.png";
    song.song.meta.base_bpm = 128.0f;
    song.song.meta.preview_start_ms = 12000;
    song.song.meta.preview_start_seconds = 12.0f;
    song.song.meta.song_version = 3;
    song.song.meta.offset = 45;
    song.song.meta.has_offset = true;
    song.song.meta.extra.unlock = {
        .unlock_state = "locked",
        .locked = true,
        .can_download = true,
        .can_play = false,
        .lock_reason = "Clear Alpha first.",
        .unlock_rule_count = 1,
    };
    song.song.meta.timing_events = {
        {timing_event_type::bpm, 0, 128.0f, 4, 4},
        {timing_event_type::meter, 960, 120.0f, 3, 4},
    };
    song.song.directory = song_dir.string();
    song.kind = content_kind::community;
    song.storage = storage_policy::managed_package;
    song.verification = verification_state::matched;
    song.status = content_status::community;
    song.source_status = content_status::community;
    song.source = content_source::community;
    song.sync_state = content_sync_state::clean;
    song.online_identity = online_content::song_identity{
        .server_url = "https://server.example",
        .remote_song_id = "remote-song-a",
        .content_source = online_content::source::community,
    };
    song.managed_manifest = song_select::managed_song_manifest_metadata{
        .package_id = "package-song-a",
        .song_json_hash = "local-song-json-sha",
        .song_json_fingerprint = "local-song-json-fingerprint",
        .audio_hash = "local-audio-sha",
        .jacket_hash = "local-jacket-sha",
        .remote_song_json_hash = "remote-song-json-sha",
        .remote_song_json_fingerprint = "remote-song-json-fingerprint",
        .remote_audio_hash = "remote-audio-sha",
        .remote_jacket_hash = "remote-jacket-sha",
        .created_at = "2026-01-02T03:04:05Z",
        .updated_at = "2026-01-02T03:05:06Z",
    };
    song.charts.push_back(make_chart("chart-a", "song-a", chart_path));
    song.song.chart_paths.push_back(chart_path.string());
    return song;
}

int main() {
    const fs::path temp_root = fs::temp_directory_path() / "raythm-local-catalog-db-smoke";
    std::error_code ec;
    fs::remove_all(temp_root, ec);
    assert(set_local_app_data(temp_root));

    const fs::path song_dir = app_paths::song_dir("song-a");
    const fs::path chart_dir = song_dir / "charts";
    const fs::path chart_path = chart_dir / "chart-a.rchart";
    fs::create_directories(chart_dir, ec);
    {
        std::ofstream(song_dir / "song.json") << "{}";
        std::ofstream(song_dir / "audio.ogg") << "audio";
        std::ofstream(song_dir / "jacket.png") << "jacket";
        std::ofstream(chart_path) << "chart";
    }

    song_select::local_catalog_database::replace_catalog({make_song(song_dir, chart_path)});
    assert(app_paths::local_catalog_cache_db_path() != app_paths::local_content_db_path());
    assert(fs::exists(app_paths::local_catalog_cache_db_path()));
    assert(!fs::exists(app_paths::local_content_db_path()));
    assert_database_stored_managed_content();

    song_select::catalog_data cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.size() == 1);
    assert(cached.songs[0].song.meta.song_id == "song-a");
    assert(cached.songs[0].song.meta.genre == "Fusion");
    assert(cached.songs[0].song.meta.duration_seconds == 95.0f);
    assert(cached.songs[0].song.meta.preview_start_seconds == 12.0f);
    assert(cached.songs[0].song.meta.offset == 45);
    assert(cached.songs[0].song.meta.has_offset);
    assert(cached.songs[0].song.meta.extra.unlock.locked);
    assert(!cached.songs[0].song.meta.extra.unlock.can_play);
    assert(cached.songs[0].song.meta.extra.unlock.lock_reason == "Clear Alpha first.");
    assert(cached.songs[0].song.meta.extra.unlock.unlock_rule_count == 1);
    assert(cached.songs[0].song.meta.timing_events.size() == 2);
    assert(cached.songs[0].song.meta.timing_events[0].type == timing_event_type::bpm);
    assert(cached.songs[0].song.meta.timing_events[0].bpm == 128.0f);
    assert(cached.songs[0].song.meta.timing_events[1].type == timing_event_type::meter);
    assert(cached.songs[0].song.meta.timing_events[1].numerator == 3);
    assert(cached.songs[0].song.meta.timing_events[1].denominator == 4);
    assert(cached.songs[0].kind == content_kind::community);
    assert(cached.songs[0].storage == storage_policy::managed_package);
    assert(cached.songs[0].verification == verification_state::matched);
    assert(cached.songs[0].status == content_status::community);
    assert(cached.songs[0].source_status == content_status::community);
    assert(cached.songs[0].source == content_source::community);
    assert(cached.songs[0].sync_state == content_sync_state::clean);
    assert(cached.songs[0].online_identity.has_value());
    assert(cached.songs[0].online_identity->remote_song_id == "remote-song-a");
    assert(cached.songs[0].managed_manifest.has_value());
    assert(cached.songs[0].managed_manifest->package_id == "package-song-a");
    assert(cached.songs[0].managed_manifest->song_json_hash == "local-song-json-sha");
    assert(cached.songs[0].managed_manifest->remote_audio_hash == "remote-audio-sha");
    assert(cached.songs[0].charts.size() == 1);
    assert(cached.songs[0].charts[0].meta.chart_id == "chart-a");
    assert(cached.songs[0].charts[0].meta.extra.unlock.locked);
    assert(!cached.songs[0].charts[0].meta.extra.unlock.can_play);
    assert(cached.songs[0].charts[0].meta.extra.unlock.lock_reason == "Clear KEY to play this chart.");
    assert(cached.songs[0].charts[0].meta.extra.unlock.unlock_rule_count == 1);
    assert(cached.songs[0].charts[0].min_bpm == 140.0f);
    assert(cached.songs[0].charts[0].max_bpm == 190.0f);
    assert(cached.songs[0].charts[0].kind == content_kind::community);
    assert(cached.songs[0].charts[0].storage == storage_policy::managed_package);
    assert(cached.songs[0].charts[0].verification == verification_state::matched);
    assert(cached.songs[0].charts[0].status == content_status::modified);
    assert(cached.songs[0].charts[0].source_status == content_status::community);
    assert(cached.songs[0].charts[0].source == content_source::community);
    assert(cached.songs[0].charts[0].sync_state == content_sync_state::modified);
    assert(cached.songs[0].charts[0].online_identity.has_value());
    assert(cached.songs[0].charts[0].online_identity->remote_chart_id == "remote-chart-a");
    assert(cached.songs[0].charts[0].managed_manifest.has_value());
    assert(cached.songs[0].charts[0].managed_manifest->chart_hash == "local-chart-sha");
    assert(cached.songs[0].charts[0].managed_manifest->remote_chart_fingerprint == "remote-chart-fingerprint");
    assert(cached.songs[0].charts[0].remote_links.empty());
    assert(cached.songs[0].song.chart_paths.size() == 1);
    const std::optional<float> cached_level =
        song_select::local_catalog_database::find_chart_level_by_path(path_utils::to_utf8(chart_path));
    assert(cached_level.has_value());
    assert(*cached_level == 7.5f);
    const std::optional<float> service_cached_level =
        song_select::local_catalog_cache_service::find_chart_level(path_utils::to_utf8(chart_path));
    assert(service_cached_level.has_value());
    assert(*service_cached_level == 7.5f);

    std::ofstream(song_dir / "notes.txt") << "not catalog metadata";
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.size() == 1);

    std::ofstream(chart_dir / "chart-b.rchart") << "new chart";
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.empty());
    fs::remove(chart_dir / "chart-b.rchart", ec);
    song_select::local_catalog_database::replace_catalog({make_song(song_dir, chart_path)});
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.size() == 1);

    {
        local_sqlite::database database = local_sqlite::open_local_catalog_cache_database();
        assert(database.valid());
        local_sqlite::put_metadata(database.get(), "local_catalog.status_schema", "online-identity-v1");
    }
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.empty());

    fs::remove_all(temp_root, ec);
    return 0;
}
