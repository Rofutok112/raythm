#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "song_loader.h"

namespace {
std::string songs_root() {
    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    return (repo_root / "assets" / "songs").string();
}
}

int main() {
    const song_load_result load_result = song_loader::load_all(songs_root());

    bool ok = true;
    if (load_result.songs.size() != 1) {
        std::cerr << "Expected exactly 1 valid song, got " << load_result.songs.size() << '\n';
        ok = false;
    }

    if (load_result.errors.size() < 2) {
        std::cerr << "Expected aggregated errors for invalid songs\n";
        ok = false;
    }

    if (ok) {
        const song_data& song = load_result.songs.front();
        if (song.meta.song_id != "sample-song") {
            std::cerr << "Unexpected song id: " << song.meta.song_id << '\n';
            ok = false;
        }

        if (song.chart_paths.size() != 1) {
            std::cerr << "Expected exactly 1 chart path, got " << song.chart_paths.size() << '\n';
            ok = false;
        } else {
            const chart_parse_result chart_result = song_loader::load_chart(song.chart_paths.front());
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
