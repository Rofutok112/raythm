#include "song_select/song_select_navigation.h"

#include <algorithm>
#include <memory>

#include "editor_scene.h"
#include "play_scene.h"
#include "settings_scene.h"
#include "song_create_scene.h"
#include "song_select/song_select_last_played.h"
#include "title_scene.h"

namespace song_select {

std::unique_ptr<scene> make_title_scene(scene_manager& manager) {
    return std::make_unique<title_scene>(manager);
}

std::unique_ptr<scene> make_settings_scene(scene_manager& manager) {
    return std::make_unique<settings_scene>(manager, settings_scene::return_target::song_select);
}

std::unique_ptr<scene> make_song_create_scene(scene_manager& manager) {
    return std::make_unique<song_create_scene>(manager);
}

std::unique_ptr<scene> make_edit_song_scene(scene_manager& manager, const song_entry& song) {
    return std::make_unique<song_create_scene>(manager, song.song);
}

std::unique_ptr<scene> make_new_chart_scene(scene_manager& manager, const song_entry& song, int difficulty_index) {
    const int key_count = song.charts.empty()
        ? 4
        : song.charts[static_cast<size_t>(std::clamp(difficulty_index, 0, static_cast<int>(song.charts.size()) - 1))].meta.key_count;
    return std::make_unique<editor_scene>(manager, song.song, key_count);
}

std::unique_ptr<scene> make_edit_chart_scene(scene_manager& manager, const song_entry& song, const chart_option& chart) {
    return std::make_unique<editor_scene>(manager, song.song, chart.path);
}

std::unique_ptr<scene> make_play_scene(scene_manager& manager, const song_entry& song, const chart_option& chart) {
    save_last_played_selection(song.song.meta.song_id, chart.meta.chart_id);
    return std::make_unique<play_scene>(manager, song.song, chart.path, chart.meta.key_count);
}

}  // namespace song_select
