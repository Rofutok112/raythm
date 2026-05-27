#include "managed_content_storage.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "app_paths.h"
#include "chart_serializer.h"
#include "mv/mv_storage.h"
#include "player_note_offsets.h"
#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_writer.h"
#include "title/local_content_index.h"

namespace fs = std::filesystem;

namespace {

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

song_meta make_song_meta(std::string song_id, std::string title) {
    song_meta meta;
    meta.song_id = std::move(song_id);
    meta.title = std::move(title);
    meta.artist = "Codex";
    meta.base_bpm = 128.0f;
    meta.audio_file = "audio.ogg";
    meta.jacket_file = "jacket.png";
    meta.preview_start_ms = 0;
    meta.song_version = 1;
    return meta;
}

chart_data make_chart(std::string chart_id, std::string song_id) {
    chart_data chart;
    chart.meta.chart_id = std::move(chart_id);
    chart.meta.song_id = std::move(song_id);
    chart.meta.key_count = 4;
    chart.meta.difficulty = "Managed";
    chart.meta.chart_author = "Codex";
    chart.meta.format_version = 1;
    chart.meta.resolution = 480;
    chart.timing_events = {
        {.type = timing_event_type::bpm, .tick = 0, .bpm = 128.0f, .numerator = 4, .denominator = 4},
        {.type = timing_event_type::meter, .tick = 0, .bpm = 0.0f, .numerator = 4, .denominator = 4},
    };
    chart.notes = {
        {.type = note_type::tap, .tick = 0, .lane = 0, .end_tick = 0},
    };
    return chart;
}

void touch_assets(const fs::path& song_dir) {
    std::ofstream(song_dir / "audio.ogg", std::ios::binary) << "audio";
    std::ofstream(song_dir / "jacket.png", std::ios::binary) << "jacket";
}

}  // namespace

namespace mv {

std::vector<mv_package> load_all_packages() {
    return {};
}

}  // namespace mv

namespace ranking_service {

listing load_chart_ranking(const std::string&, source ranking_source, int) {
    listing result;
    result.ranking_source = ranking_source;
    return result;
}

}  // namespace ranking_service

player_chart_offset_map load_player_chart_offsets() {
    return {};
}

int main() {
    const fs::path temp_root = fs::temp_directory_path() / "raythm-managed-content-storage-smoke";
    std::error_code ec;
    fs::remove_all(temp_root, ec);
    assert(set_local_app_data(temp_root));

    app_paths::ensure_directories();

    const fs::path legacy_song_dir = app_paths::song_dir("remote-song");
    assert(song_writer::write_song_json(make_song_meta("remote-song", "Legacy Local"), legacy_song_dir.string()));
    touch_assets(legacy_song_dir);

    const managed_content_storage::song_identity song_identity{
        .source = online_content::source::community,
        .server_url = "https://server.example/api",
        .remote_song_id = "remote-song",
        .song_version = 2,
        .revision_id = "song-rev-2",
        .package_id = "package-remote-song",
    };
    const managed_content_storage::chart_identity chart_identity{
        .source = online_content::source::community,
        .server_url = song_identity.server_url,
        .remote_song_id = song_identity.remote_song_id,
        .remote_chart_id = "remote-chart",
        .song_version = song_identity.song_version,
        .chart_version = 5,
        .revision_id = "chart-rev-5",
        .chart_hash = "local-chart-sha",
        .chart_fingerprint = "local-chart-fingerprint",
        .remote_chart_hash = "remote-chart-sha",
        .remote_chart_fingerprint = "remote-chart-fingerprint",
    };

    const std::string managed_song_id = managed_content_storage::local_song_id(song_identity);
    const std::string managed_chart_id = managed_content_storage::local_chart_id(chart_identity);
    assert(managed_song_id != "remote-song");
    assert(managed_chart_id != "remote-chart");

    const fs::path managed_song_dir = managed_content_storage::song_directory(song_identity);
    assert(song_writer::write_song_json(make_song_meta(managed_song_id, "Managed Remote"), managed_song_dir.string()));
    touch_assets(managed_song_dir);
    fs::create_directories(managed_song_dir / "charts", ec);
    assert(chart_serializer::serialize(
        make_chart(managed_chart_id, managed_song_id),
        (managed_song_dir / "charts" / (managed_chart_id + ".rchart")).string()));

    managed_content_storage::package_manifest manifest{
        .song = song_identity,
        .local_song_id = managed_song_id,
        .song_json_hash = "local-song-json-sha",
        .song_json_fingerprint = "local-song-json-fingerprint",
        .audio_hash = "local-audio-sha",
        .jacket_hash = "local-jacket-sha",
        .remote_song_json_hash = "remote-song-json-sha",
        .remote_song_json_fingerprint = "remote-song-json-fingerprint",
        .remote_audio_hash = "remote-audio-sha",
        .remote_jacket_hash = "remote-jacket-sha",
    };
    managed_content_storage::upsert_chart(manifest, chart_identity);
    std::string error_message;
    assert(managed_content_storage::write_manifest(manifest, error_message));
    const std::optional<managed_content_storage::package_manifest> stored_manifest =
        managed_content_storage::read_manifest(managed_song_dir);
    assert(stored_manifest.has_value());
    assert(stored_manifest->song.source == online_content::source::community);
    assert(stored_manifest->song.package_id == "package-remote-song");
    assert(stored_manifest->song_json_hash == "local-song-json-sha");
    assert(stored_manifest->song_json_fingerprint == "local-song-json-fingerprint");
    assert(stored_manifest->audio_hash == "local-audio-sha");
    assert(stored_manifest->jacket_hash == "local-jacket-sha");
    assert(stored_manifest->remote_song_json_hash == "remote-song-json-sha");
    assert(stored_manifest->remote_song_json_fingerprint == "remote-song-json-fingerprint");
    assert(stored_manifest->remote_audio_hash == "remote-audio-sha");
    assert(stored_manifest->remote_jacket_hash == "remote-jacket-sha");
    assert(!stored_manifest->created_at.empty());
    assert(!stored_manifest->updated_at.empty());
    assert(stored_manifest->charts.size() == 1);
    assert(stored_manifest->charts.front().chart_hash == "local-chart-sha");
    assert(stored_manifest->charts.front().chart_fingerprint == "local-chart-fingerprint");
    assert(stored_manifest->charts.front().remote_chart_hash == "remote-chart-sha");
    assert(stored_manifest->charts.front().remote_chart_fingerprint == "remote-chart-fingerprint");

    local_content_index::put_song_binding({
        .server_url = song_identity.server_url,
        .local_song_id = managed_song_id,
        .remote_song_id = song_identity.remote_song_id,
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = song_identity.server_url,
        .local_chart_id = managed_chart_id,
        .remote_chart_id = chart_identity.remote_chart_id,
        .remote_song_id = chart_identity.remote_song_id,
        .remote_chart_version = chart_identity.chart_version,
        .origin = local_content_index::online_origin::downloaded,
    });

    const song_select::catalog_data catalog = song_select::load_catalog(true);
    assert(catalog.songs.size() == 2);

    const song_select::song_entry* legacy = nullptr;
    const song_select::song_entry* managed = nullptr;
    for (const song_select::song_entry& song : catalog.songs) {
        if (song.song.meta.song_id == "remote-song") {
            legacy = &song;
        }
        if (song.song.meta.song_id == managed_song_id) {
            managed = &song;
        }
    }

    assert(legacy != nullptr);
    assert(legacy->storage == storage_policy::plain_workspace);
    assert(legacy->song.directory.find("content-cache") == std::string::npos);

    assert(managed != nullptr);
    assert(managed->storage == storage_policy::managed_package);
    assert(managed->source_status == content_status::community);
    assert(managed->online_identity.has_value());
    assert(managed->online_identity->remote_song_id == "remote-song");
    assert(managed->managed_manifest.has_value());
    assert(managed->managed_manifest->package_id == "package-remote-song");
    assert(managed->managed_manifest->song_json_hash == "local-song-json-sha");
    assert(managed->managed_manifest->audio_hash == "local-audio-sha");
    assert(managed->song.directory.find("content-cache") != std::string::npos);
    assert(managed->song.directory.find("community") != std::string::npos);
    assert(managed->charts.size() == 1);
    assert(managed->charts.front().storage == storage_policy::managed_package);
    assert(managed->charts.front().online_identity.has_value());
    assert(managed->charts.front().online_identity->remote_chart_id == "remote-chart");
    assert(managed->charts.front().managed_manifest.has_value());
    assert(managed->charts.front().managed_manifest->chart_hash == "local-chart-sha");
    assert(managed->charts.front().managed_manifest->remote_chart_fingerprint == "remote-chart-fingerprint");
    assert(managed->charts.front().path.find("content-cache") != std::string::npos);

    fs::remove_all(temp_root, ec);
    std::cout << "managed_content_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
