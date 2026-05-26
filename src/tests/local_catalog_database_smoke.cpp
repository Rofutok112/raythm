#include "app_paths.h"
#include "local_sqlite.h"
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

void assert_database_stored_local_plain_workspace() {
    local_sqlite::database database = local_sqlite::open_local_content_database();
    assert(database.valid());

    local_sqlite::statement song(database.get(),
                                 "SELECT status, source_status, content_kind, storage_policy, verification_state, "
                                 "online_server_url, online_song_id, online_source "
                                 "FROM local_songs WHERE song_id = 'song-a';");
    assert(song.valid());
    assert(sqlite3_step(song.get()) == SQLITE_ROW);
    assert(local_sqlite::column_text(song.get(), 0) == "local");
    assert(local_sqlite::column_text(song.get(), 1) == "local");
    assert(local_sqlite::column_text(song.get(), 2) == "local");
    assert(local_sqlite::column_text(song.get(), 3) == "plain_workspace");
    assert(local_sqlite::column_text(song.get(), 4) == "unchecked");
    assert(local_sqlite::column_text(song.get(), 5).empty());
    assert(local_sqlite::column_text(song.get(), 6).empty());
    assert(local_sqlite::column_text(song.get(), 7).empty());

    local_sqlite::statement chart(database.get(),
                                  "SELECT status, source_status, content_kind, storage_policy, verification_state, "
                                  "online_server_url, online_song_id, online_chart_id, online_source, online_chart_version "
                                  "FROM local_charts WHERE chart_id = 'chart-a';");
    assert(chart.valid());
    assert(sqlite3_step(chart.get()) == SQLITE_ROW);
    assert(local_sqlite::column_text(chart.get(), 0) == "local");
    assert(local_sqlite::column_text(chart.get(), 1) == "local");
    assert(local_sqlite::column_text(chart.get(), 2) == "local");
    assert(local_sqlite::column_text(chart.get(), 3) == "plain_workspace");
    assert(local_sqlite::column_text(chart.get(), 4) == "unchecked");
    assert(local_sqlite::column_text(chart.get(), 5).empty());
    assert(local_sqlite::column_text(chart.get(), 6).empty());
    assert(local_sqlite::column_text(chart.get(), 7).empty());
    assert(local_sqlite::column_text(chart.get(), 8).empty());
    assert(sqlite3_column_int(chart.get(), 9) == 0);
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
    chart.status = content_status::community;
    chart.source_status = content_status::community;
    chart.online_identity = online_content::chart_identity{
        .server_url = "https://server.example",
        .remote_song_id = "remote-song-a",
        .remote_chart_id = "remote-chart-a",
        .content_source = online_content::source::community,
        .remote_chart_version = 7,
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
    song.online_identity = online_content::song_identity{
        .server_url = "https://server.example",
        .remote_song_id = "remote-song-a",
        .content_source = online_content::source::community,
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
    assert_database_stored_local_plain_workspace();

    song_select::catalog_data cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.size() == 1);
    assert(cached.songs[0].song.meta.song_id == "song-a");
    assert(cached.songs[0].song.meta.genre == "Fusion");
    assert(cached.songs[0].song.meta.duration_seconds == 95.0f);
    assert(cached.songs[0].song.meta.preview_start_seconds == 12.0f);
    assert(cached.songs[0].song.meta.offset == 45);
    assert(cached.songs[0].song.meta.has_offset);
    assert(cached.songs[0].song.meta.timing_events.size() == 2);
    assert(cached.songs[0].song.meta.timing_events[0].type == timing_event_type::bpm);
    assert(cached.songs[0].song.meta.timing_events[0].bpm == 128.0f);
    assert(cached.songs[0].song.meta.timing_events[1].type == timing_event_type::meter);
    assert(cached.songs[0].song.meta.timing_events[1].numerator == 3);
    assert(cached.songs[0].song.meta.timing_events[1].denominator == 4);
    assert(cached.songs[0].kind == content_kind::local);
    assert(cached.songs[0].storage == storage_policy::plain_workspace);
    assert(cached.songs[0].verification == verification_state::unchecked);
    assert(cached.songs[0].status == content_status::local);
    assert(cached.songs[0].source_status == content_status::local);
    assert(!cached.songs[0].online_identity.has_value());
    assert(cached.songs[0].charts.size() == 1);
    assert(cached.songs[0].charts[0].meta.chart_id == "chart-a");
    assert(cached.songs[0].charts[0].min_bpm == 140.0f);
    assert(cached.songs[0].charts[0].max_bpm == 190.0f);
    assert(cached.songs[0].charts[0].kind == content_kind::local);
    assert(cached.songs[0].charts[0].storage == storage_policy::plain_workspace);
    assert(cached.songs[0].charts[0].verification == verification_state::unchecked);
    assert(cached.songs[0].charts[0].status == content_status::local);
    assert(cached.songs[0].charts[0].source_status == content_status::local);
    assert(!cached.songs[0].charts[0].online_identity.has_value());
    assert(cached.songs[0].charts[0].remote_links.empty());
    assert(cached.songs[0].song.chart_paths.size() == 1);

    {
        local_sqlite::database database = local_sqlite::open_local_content_database();
        assert(database.valid());
        local_sqlite::put_metadata(database.get(), "local_catalog.status_schema", "online-identity-v1");
    }
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.empty());

    std::ofstream(chart_dir / "chart-b.rchart") << "new chart";
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.empty());

    fs::remove_all(temp_root, ec);
    return 0;
}
