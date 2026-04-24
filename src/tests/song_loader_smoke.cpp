#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "chart_difficulty.h"
#include "song_loader.h"

namespace {
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
           << "songId=song-loader-song\n"
           << "isPublic=false\n\n"
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
}

int main() {
    bool ok = true;
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
        const float expected_level = chart_difficulty::calculate_level(*chart_result.data);
        if (chart_result.data->meta.level == 55.0f ||
            chart_result.data->meta.level != expected_level) {
            std::cerr << "Expected loaded chart level to be normalized to auto level\n";
            ok = false;
        }
    }
    std::filesystem::remove(temp_chart);

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
                const float expected_level = chart_difficulty::calculate_level(*loaded_chart.data);
                if (loaded_chart.data->meta.level != expected_level) {
                    std::cerr << "Expected loaded chart level to be normalized to auto level\n";
                    ok = false;
                }
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
