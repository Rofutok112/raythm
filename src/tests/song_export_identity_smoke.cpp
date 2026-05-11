#include <cstdlib>
#include <iostream>

#include "song_select/song_export_identity.h"

namespace {

bool looks_like_uuid_v4(const std::string& value) {
    return value.size() == 36 &&
           value[8] == '-' &&
           value[13] == '-' &&
           value[14] == '4' &&
           value[18] == '-' &&
           value[23] == '-';
}

}  // namespace

int main() {
    bool ok = true;

    chart_data chart;
    chart.meta.chart_id = "original-chart";
    chart.meta.song_id = "original-song";
    chart.meta.difficulty = "Hard";
    chart.meta.level = 12.0f;
    chart.notes.push_back(note_data{.type = note_type::tap, .tick = 120, .lane = 1});

    const chart_data exported_chart = song_select::make_export_chart_copy(chart);
    if (exported_chart.meta.chart_id.empty() ||
        exported_chart.meta.chart_id == chart.meta.chart_id ||
        !looks_like_uuid_v4(exported_chart.meta.chart_id)) {
        std::cerr << "Expected exported chart to receive a new chartId\n";
        ok = false;
    }
    if (!exported_chart.meta.song_id.empty()) {
        std::cerr << "Expected exported chart package metadata to avoid binding to the source songId\n";
        ok = false;
    }
    if (chart.meta.chart_id != "original-chart" || chart.meta.song_id != "original-song") {
        std::cerr << "Expected source chart metadata to remain unchanged\n";
        ok = false;
    }
    if (exported_chart.meta.difficulty != chart.meta.difficulty ||
        exported_chart.meta.level != chart.meta.level ||
        exported_chart.notes.size() != chart.notes.size()) {
        std::cerr << "Expected exported chart copy to preserve authored chart data\n";
        ok = false;
    }

    song_meta song;
    song.song_id = "original-song";
    song.title = "Original Song";
    song.artist = "Codex";
    song.audio_file = "audio.ogg";
    song.jacket_file = "jacket.png";

    const song_meta exported_song = song_select::make_export_song_meta_copy(song);
    if (exported_song.song_id.empty() ||
        exported_song.song_id == song.song_id ||
        !looks_like_uuid_v4(exported_song.song_id)) {
        std::cerr << "Expected exported song to receive a new songId\n";
        ok = false;
    }
    if (song.song_id != "original-song") {
        std::cerr << "Expected source song metadata to remain unchanged\n";
        ok = false;
    }
    if (exported_song.title != song.title ||
        exported_song.artist != song.artist ||
        exported_song.audio_file != song.audio_file ||
        exported_song.jacket_file != song.jacket_file) {
        std::cerr << "Expected exported song copy to preserve distributable metadata\n";
        ok = false;
    }

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "song_export_identity smoke test passed\n";
    return EXIT_SUCCESS;
}
