#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "chart_identity_store.h"
#include "song_fingerprint.h"
#include "song_loader.h"
#include "song_writer.h"
#include "updater/update_verify.h"

namespace {
bool set_local_app_data(const std::filesystem::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

std::string songs_root() {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (repo_root / "assets" / "songs").string();
}

size_t expected_song_count() {
    const std::filesystem::path root = songs_root();
    if (!std::filesystem::exists(root)) {
        return 0;
    }

    size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "song.json")) {
            ++count;
        }
    }
    return count;
}

std::filesystem::path write_temp_chart() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_song_loader_auto_level.rchart";
    std::ofstream output(path, std::ios::trunc);
    output << "[Metadata]\n"
           << "chartId=song-loader-auto-level\n"
           << "keyCount=4\n"
           << "difficulty=Legacy\n"
           << "level=55.0\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=1\n"
           << "resolution=480\n"
           << "offset=0\n"
           << "songId=song-loader-song\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,0\n"
           << "tap,240,1\n"
           << "tap,480,2\n"
           << "tap,720,3\n";
    return path;
}

std::filesystem::path write_temp_song_without_embedded_id() {
    const std::filesystem::path song_dir =
        std::filesystem::temp_directory_path() / "raythm_song_loader_external_song_id";
    std::error_code ec;
    std::filesystem::remove_all(song_dir, ec);
    std::filesystem::create_directories(song_dir);
    std::ofstream output(song_dir / "song.json", std::ios::trunc);
    output << "{\n"
           << "  \"title\": \"External ID Song\",\n"
           << "  \"artist\": \"Codex\",\n"
           << "  \"baseBpm\": 120,\n"
           << "  \"audioFile\": \"audio.ogg\",\n"
           << "  \"jacketFile\": \"jacket.png\",\n"
           << "  \"previewStartMs\": 0,\n"
           << "  \"songVersion\": 1\n"
           << "}\n";
    return song_dir;
}

std::filesystem::path write_temp_song_with_legacy_preview_fields() {
    const std::filesystem::path song_dir =
        std::filesystem::temp_directory_path() / "raythm_song_loader_legacy_preview";
    std::error_code ec;
    std::filesystem::remove_all(song_dir, ec);
    std::filesystem::create_directories(song_dir);
    std::ofstream output(song_dir / "song.json", std::ios::trunc);
    output << "{\n"
           << "  \"title\": \"Preview Priority Song\",\n"
           << "  \"artist\": \"Codex\",\n"
           << "  \"baseBpm\": 120,\n"
           << "  \"audioFile\": \"audio.ogg\",\n"
           << "  \"jacketFile\": \"jacket.png\",\n"
           << "  \"chorusStartSeconds\": 9,\n"
           << "  \"previewStartSeconds\": 8,\n"
           << "  \"songVersion\": 1\n"
           << "}\n";
    return song_dir;
}

std::filesystem::path write_external_chart_without_song_id() {
    const std::filesystem::path charts_dir =
        std::filesystem::temp_directory_path() / "raythm_song_loader_external_charts";
    std::error_code ec;
    std::filesystem::remove_all(charts_dir, ec);
    std::filesystem::create_directories(charts_dir);
    std::ofstream output(charts_dir / "external-linked.rchart", std::ios::trunc);
    output << "[Metadata]\n"
           << "keyCount=4\n"
           << "difficulty=External\n"
           << "chartAuthor=Codex\n"
           << "formatVersion=1\n"
           << "resolution=480\n"
           << "offset=0\n\n"
           << "[Timing]\n"
           << "bpm,0,120\n"
           << "meter,0,4/4\n\n"
           << "[Notes]\n"
           << "tap,0,0\n";
    return charts_dir;
}
}

int main() {
    bool ok = true;
    const std::filesystem::path appdata_root =
        std::filesystem::temp_directory_path() / "raythm-song-loader-smoke-appdata";
    std::error_code ec;
    std::filesystem::remove_all(appdata_root, ec);
    if (!set_local_app_data(appdata_root)) {
        std::cerr << "failed to set LOCALAPPDATA\n";
        return EXIT_FAILURE;
    }

    song_load_result load_result;
    if (std::filesystem::exists(songs_root())) {
        load_result = song_loader::load_all(songs_root());

        if (load_result.songs.size() != expected_song_count()) {
            std::cerr << "Expected every song package under assets/songs to load, got "
                      << load_result.songs.size() << '\n';
            ok = false;
        }

        if (!load_result.errors.empty()) {
            std::cerr << "Expected no song loading errors\n";
            ok = false;
        }

        if (ok) {
            const song_data* song_with_chart = nullptr;
            for (const song_data& candidate : load_result.songs) {
                if (!candidate.chart_paths.empty()) {
                    song_with_chart = &candidate;
                    break;
                }
            }

            if (song_with_chart != nullptr) {
                const chart_parse_result chart_result = song_loader::load_chart(song_with_chart->chart_paths.front());
                if (!chart_result.success || !chart_result.data.has_value()) {
                    std::cerr << "Expected deferred chart load to succeed\n";
                    for (const std::string& error : chart_result.errors) {
                        std::cerr << "  " << error << '\n';
                    }
                    ok = false;
                }
            }
        }
    }

    const std::filesystem::path temp_chart = write_temp_chart();
    const chart_parse_result chart_result = song_loader::load_chart(temp_chart.string());
    if (!chart_result.success || !chart_result.data.has_value()) {
        std::cerr << "Expected temp chart load to succeed\n";
        for (const std::string& error : chart_result.errors) {
            std::cerr << "  " << error << '\n';
        }
        ok = false;
    } else {
        if (chart_result.data->meta.level != 0.0f) {
            std::cerr << "Expected legacy chart level metadata to be ignored by the loader\n";
            ok = false;
        }
    }
    std::filesystem::remove(temp_chart);

    const std::filesystem::path temp_song = write_temp_song_without_embedded_id();
    const song_load_result temp_song_result = song_loader::load_directory(temp_song.string());
    if (temp_song_result.songs.size() != 1 ||
        temp_song_result.songs.front().meta.song_id != "raythm_song_loader_external_song_id") {
        std::cerr << "Expected missing songId to fall back to the song directory name\n";
        ok = false;
    }
    std::filesystem::remove_all(temp_song);

    const std::filesystem::path legacy_preview_song = write_temp_song_with_legacy_preview_fields();
    const song_load_result legacy_preview_result = song_loader::load_directory(legacy_preview_song.string());
    if (!legacy_preview_result.songs.empty() ||
        legacy_preview_result.errors.empty()) {
        std::cerr << "Expected legacy preview fields without previewStartMs to be rejected\n";
        ok = false;
    }
    std::filesystem::remove_all(legacy_preview_song);

    const std::filesystem::path written_song_dir =
        std::filesystem::temp_directory_path() / "raythm_song_writer_external_id";
    std::filesystem::remove_all(written_song_dir, ec);
    song_meta written_meta;
    written_meta.song_id = "external-local-id";
    written_meta.title = "Written Song";
    written_meta.artist = "Codex";
    written_meta.base_bpm = 128.0f;
    written_meta.audio_file = "audio.ogg";
    written_meta.jacket_file = "jacket.png";
    written_meta.preview_start_ms = 0;
    written_meta.song_version = 1;
    if (!song_writer::write_song_json(written_meta, written_song_dir.string())) {
        std::cerr << "Expected song writer to write song.json\n";
        ok = false;
    } else {
        std::ifstream written_input(written_song_dir / "song.json", std::ios::binary);
        const std::string written_content{std::istreambuf_iterator<char>(written_input),
                                          std::istreambuf_iterator<char>()};
        if (written_content.find("\"songId\": \"external-local-id\"") == std::string::npos) {
            std::cerr << "Expected song writer to persist songId in song.json\n";
            ok = false;
        }
    }
    std::filesystem::remove_all(written_song_dir, ec);

    const std::string song_json_a =
        "{\n  \"songId\": \"local-song\",\n  \"title\": \"Same\",\n  \"artist\": \"Codex\"\n}\n";
    const std::string song_json_b =
        "{\n  \"songId\": \"remote-song\",\n  \"title\": \"Same\",\n  \"artist\": \"Codex\"\n}\n";
    const std::string song_fp_a = song_fingerprint::build(song_json_a);
    const std::string song_fp_b = song_fingerprint::build(song_json_b);
    if (updater::compute_sha256_hex(std::string_view(song_fp_a)) !=
        updater::compute_sha256_hex(std::string_view(song_fp_b))) {
        std::cerr << "Expected song fingerprint to ignore songId differences\n";
        ok = false;
    }
    const std::string song_json_local =
        "{\n"
        "  \"songId\": \"local-song\",\n"
        "  \"title\": \"Same\",\n"
        "  \"artist\": \"Codex\",\n"
        "  \"baseBpm\": 125.0,\n"
        "  \"audioFile\": \"custom-audio.ogg\",\n"
        "  \"jacketFile\": \"custom-jacket.png\",\n"
        "  \"previewStartMs\": 1200,\n"
        "  \"songVersion\": 1\n"
        "}\n";
    const std::string song_json_server =
        "{\n"
        "  \"artist\": \"Codex\",\n"
        "  \"audioFile\": \"audio.ogg\",\n"
        "  \"baseBpm\": 125,\n"
        "  \"jacketFile\": \"jacket.png\",\n"
        "  \"previewStartMs\": 1200,\n"
        "  \"songVersion\": 1,\n"
        "  \"title\": \"Same\"\n"
        "}\n";
    if (updater::compute_sha256_hex(std::string_view(song_fingerprint::build(song_json_local))) !=
        updater::compute_sha256_hex(std::string_view(song_fingerprint::build(song_json_server)))) {
        std::cerr << "Expected song fingerprint to ignore storage-only song metadata differences\n";
        ok = false;
    }
    const std::string escaped_song_json =
        "{\n"
        "  \"title\": \"Same \\\"Title\\\"\",\n"
        "  \"artist\": \"Codex\",\n"
        "  \"baseBpm\": 125,\n"
        "  \"previewStartMs\": 0,\n"
        "  \"songVersion\": 1\n"
        "}\n";
    if (song_fingerprint::build(escaped_song_json).find("title=Same \"Title\"") == std::string::npos) {
        std::cerr << "Expected song fingerprint to normalize escaped JSON strings\n";
        ok = false;
    }

    const std::filesystem::path external_charts_dir = write_external_chart_without_song_id();
    chart_identity::put("external-linked", "linked-song");
    std::vector<song_data> external_songs;
    song_data linked_song;
    linked_song.meta.song_id = "linked-song";
    external_songs.push_back(linked_song);
    song_loader::attach_external_charts(external_charts_dir.string(), external_songs);
    if (external_songs.front().chart_paths.empty()) {
        std::cerr << "Expected external chart identity index to attach chart to song\n";
        ok = false;
    }
    std::filesystem::remove_all(external_charts_dir);
    std::filesystem::remove_all(appdata_root, ec);

    if (std::filesystem::exists(songs_root()) && ok) {
        const song_data* song_with_chart = nullptr;
        for (const song_data& candidate : load_result.songs) {
            if (!candidate.chart_paths.empty()) {
                song_with_chart = &candidate;
                break;
            }
        }

        if (song_with_chart == nullptr) {
            std::cerr << "Expected at least one loaded song to have a chart\n";
            ok = false;
        } else {
            const chart_parse_result loaded_chart = song_loader::load_chart(song_with_chart->chart_paths.front());
            if (!loaded_chart.success || !loaded_chart.data.has_value()) {
                std::cerr << "Expected deferred chart load to succeed\n";
                ok = false;
            } else {
            }
        }
    }

    if (!ok) {
        for (const std::string& error : load_result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return EXIT_FAILURE;
    }

    std::cout << "song_loader smoke test passed\n";
    return EXIT_SUCCESS;
}
