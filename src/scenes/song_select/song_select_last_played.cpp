#include "song_select/song_select_last_played.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app_paths.h"

namespace {

std::filesystem::path last_played_path() {
    return app_paths::app_data_root() / "last_played_chart.txt";
}

}  // namespace

namespace song_select {

last_played_selection load_last_played_selection() {
    std::ifstream input(last_played_path());
    if (!input.is_open()) {
        return {};
    }

    last_played_selection result;
    std::getline(input, result.song_id);
    std::getline(input, result.chart_id);
    return result;
}

bool save_last_played_selection(const std::string& song_id, const std::string& chart_id) {
    if (song_id.empty() || chart_id.empty()) {
        return false;
    }

    app_paths::ensure_directories();
    std::ofstream output(last_played_path(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << song_id << '\n' << chart_id << '\n';
    return output.good();
}

}  // namespace song_select
