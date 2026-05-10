#include "app_paths.h"
#include "song_select/local_catalog_database.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace fs = std::filesystem;

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
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
    chart.status = content_status::community;
    chart.source_status = content_status::community;
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
    song.song.directory = song_dir.string();
    song.status = content_status::community;
    song.source_status = content_status::community;
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

    song_select::catalog_data cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.size() == 1);
    assert(cached.songs[0].song.meta.song_id == "song-a");
    assert(cached.songs[0].song.meta.genre == "Fusion");
    assert(cached.songs[0].song.meta.duration_seconds == 95.0f);
    assert(cached.songs[0].song.meta.preview_start_seconds == 12.0f);
    assert(cached.songs[0].status == content_status::community);
    assert(cached.songs[0].source_status == content_status::community);
    assert(cached.songs[0].charts.size() == 1);
    assert(cached.songs[0].charts[0].meta.chart_id == "chart-a");
    assert(cached.songs[0].charts[0].min_bpm == 140.0f);
    assert(cached.songs[0].charts[0].max_bpm == 190.0f);
    assert(cached.songs[0].charts[0].status == content_status::community);
    assert(cached.songs[0].charts[0].source_status == content_status::community);
    assert(cached.songs[0].song.chart_paths.size() == 1);

    std::ofstream(chart_dir / "chart-b.rchart") << "new chart";
    cached = song_select::local_catalog_database::load_cached_catalog();
    assert(cached.songs.empty());

    fs::remove_all(temp_root, ec);
    return 0;
}
