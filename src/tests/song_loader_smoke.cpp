#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "song_loader.h"

namespace {
std::string songs_root() {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (repo_root / "assets" / "songs").string();
}

size_t expected_song_count() {
    const std::filesystem::path root = songs_root();
    size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "song.json")) {
            ++count;
        }
    }
    return count;
}
}

int main() {
    const song_load_result load_result = song_loader::load_all(songs_root());

    bool ok = true;
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

        if (song_with_chart == nullptr) {
            std::cerr << "Expected at least one loaded song to have a chart\n";
            ok = false;
        } else {
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

    if (!ok) {
        for (const std::string& error : load_result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return EXIT_FAILURE;
    }

    std::cout << "song_loader smoke test passed\n";
    return EXIT_SUCCESS;
}
