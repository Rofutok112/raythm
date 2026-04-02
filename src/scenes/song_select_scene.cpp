#include "song_select_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

#include "app_paths.h"
#include "editor_scene.h"
#include "game_settings.h"

#include "path_utils.h"
#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_scene.h"
#include "song_create_scene.h"
#include "song_loader.h"
#include "theme.h"
#include "title_scene.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
constexpr ui::draw_layer kSongSelectLayer = ui::draw_layer::base;
constexpr float kRowHeight = 60.0f;
constexpr float kScrollWheelStep = 80.0f;
constexpr float kScrollLerpSpeed = 12.0f;
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kSettingsButtonRect = ui::place(kScreenRect, 162.0f, 30.0f,
                                                    ui::anchor::top_right, ui::anchor::top_right,
                                                    {-24.0f, 8.0f});
constexpr Rectangle kSongListRect = ui::place(kScreenRect, 466.0f, 660.0f,
                                              ui::anchor::top_right, ui::anchor::top_right,
                                              {-24.0f, 44.0f});
constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 750.0f, 660.0f,
                                               ui::anchor::top_left, ui::anchor::top_left,
                                               {24.0f, 44.0f});
constexpr Rectangle kJacketRect = ui::place(kLeftPanelRect, 320.0f, 320.0f,
                                            ui::anchor::top_left, ui::anchor::top_left,
                                            {20.0f, 24.0f});
constexpr Rectangle kSceneTitleRect = ui::place(kScreenRect, 360.0f, 30.0f,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                {30.0f, 12.0f});
constexpr Rectangle kSongListTitleRect = ui::place(kSongListRect, 180.0f, 28.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {20.0f, 10.0f});
constexpr Rectangle kSongListNewSongButtonRect = ui::place(kSongListRect, 124.0f, 30.0f,
                                                           ui::anchor::top_right, ui::anchor::top_right,
                                                           {-16.0f, 10.0f});
constexpr float kSongListHeaderHeight = 48.0f;
constexpr float kSongListBottomPadding = 12.0f;
constexpr Rectangle kSongListViewRect = ui::scroll_view(kSongListRect, kSongListHeaderHeight, kSongListBottomPadding);
constexpr Rectangle kSongListScrollbarTrackRect = ui::place(kSongListViewRect, 6.0f, kSongListViewRect.height,
                                                            ui::anchor::top_right, ui::anchor::top_right,
                                                            {-8.0f, 0.0f});
constexpr float kPreviewFadeSpeed = 2.4f;
constexpr float kPreviewMaxVolume = 0.55f;
constexpr ui::draw_layer kContextMenuLayer = ui::draw_layer::overlay;
constexpr ui::draw_layer kModalLayer = ui::draw_layer::modal;
constexpr float kContextMenuWidth = 220.0f;
constexpr float kContextMenuItemHeight = 30.0f;
constexpr float kContextMenuItemSpacing = 4.0f;
constexpr Rectangle kConfirmDialogRect = ui::place(kScreenRect, 480.0f, 208.0f,
                                                   ui::anchor::center, ui::anchor::center);

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Rectangle make_context_menu_rect(Vector2 anchor, int item_count) {
    const float height = 12.0f + static_cast<float>(item_count) * kContextMenuItemHeight +
                         static_cast<float>(std::max(0, item_count - 1)) * kContextMenuItemSpacing;
    Rectangle rect = {anchor.x, anchor.y, kContextMenuWidth, height};
    rect.x = std::clamp(rect.x, 12.0f, kScreenRect.width - rect.width - 12.0f);
    rect.y = std::clamp(rect.y, 12.0f, kScreenRect.height - rect.height - 12.0f);
    return rect;
}

bool is_within_root(const std::filesystem::path& path, const std::filesystem::path& root) {
    std::error_code ec;
    const std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path normalized_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }

    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }

    return true;
}

const char* source_label(content_source source) {
    return source == content_source::app_data ? "AppData" : "Legacy";
}

}

song_select_scene::song_select_scene(scene_manager& manager, std::string preferred_song_id)
    : scene(manager), preferred_song_id_(std::move(preferred_song_id)) {
}

void song_select_scene::on_enter() {
    selected_song_index_ = 0;
    difficulty_index_ = 0;
    scroll_y_ = 0.0f;
    scroll_y_target_ = 0.0f;
    song_change_anim_t_ = 1.0f;
    scene_fade_in_t_ = 1.0f;
    context_menu_ = {};
    confirmation_dialog_ = {};
    reload_song_library(preferred_song_id_);
}

void song_select_scene::on_exit() {
    close_context_menu();
    confirmation_dialog_ = {};
    stop_preview_and_unload_jacket();
}

const song_select_scene::song_entry* song_select_scene::selected_song() const {
    if (songs_.empty() || selected_song_index_ < 0 || selected_song_index_ >= static_cast<int>(songs_.size())) {
        return nullptr;
    }

    return &songs_[static_cast<size_t>(selected_song_index_)];
}

std::vector<const song_select_scene::chart_option*> song_select_scene::filtered_charts_for_selected_song() const {
    std::vector<const chart_option*> filtered;
    const song_entry* song = selected_song();
    if (song == nullptr) {
        return filtered;
    }

    for (const chart_option& chart : song->charts) {
        filtered.push_back(&chart);
    }

    return filtered;
}

void song_select_scene::reload_song_library(const std::string& preferred_song_id,
                                            const std::string& preferred_chart_id) {
    songs_.clear();
    load_errors_.clear();

    const song_load_result legacy_result = song_loader::load_all(path_utils::to_utf8(app_paths::legacy_songs_root()),
                                                                 content_source::legacy_assets);
    const song_load_result appdata_result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()),
                                                                  content_source::app_data);
    load_errors_ = legacy_result.errors;
    load_errors_.insert(load_errors_.end(), appdata_result.errors.begin(), appdata_result.errors.end());

    std::vector<song_data> all_songs = legacy_result.songs;
    all_songs.insert(all_songs.end(), appdata_result.songs.begin(), appdata_result.songs.end());
    song_loader::attach_external_charts(path_utils::to_utf8(app_paths::charts_root()), all_songs);

    std::sort(all_songs.begin(), all_songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    for (const song_data& song : all_songs) {
        song_entry entry;
        entry.song = song;

        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            const content_source chart_source = song_loader::classify_chart_path(chart_path);
            entry.charts.push_back({
                chart_path,
                parse_result.data->meta,
                chart_source,
                chart_source == content_source::app_data
            });
        }

        std::sort(entry.charts.begin(), entry.charts.end(), [](const chart_option& left, const chart_option& right) {
            if (left.meta.key_count != right.meta.key_count) {
                return left.meta.key_count < right.meta.key_count;
            }
            if (left.meta.level != right.meta.level) {
                return left.meta.level < right.meta.level;
            }
            return left.meta.difficulty < right.meta.difficulty;
        });

        songs_.push_back(std::move(entry));
    }

    selected_song_index_ = 0;
    difficulty_index_ = 0;
    scroll_y_ = 0.0f;
    scroll_y_target_ = 0.0f;

    if (!preferred_song_id.empty()) {
        for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
            if (songs_[static_cast<size_t>(i)].song.meta.song_id == preferred_song_id) {
                selected_song_index_ = i;
                break;
            }
        }
    }

    if (!songs_.empty() && !preferred_chart_id.empty()) {
        const auto& charts = songs_[static_cast<size_t>(selected_song_index_)].charts;
        for (int i = 0; i < static_cast<int>(charts.size()); ++i) {
            if (charts[static_cast<size_t>(i)].meta.chart_id == preferred_chart_id) {
                difficulty_index_ = i;
                break;
            }
        }
    }

    if (songs_.empty()) {
        stop_preview_and_unload_jacket();
        return;
    }

    song_change_anim_t_ = 1.0f;
    queue_preview_for_selected_song();
    load_jacket_for_selected_song();
}

void song_select_scene::load_jacket_for_selected_song() {
    if (jacket_loaded_) {
        UnloadTexture(jacket_texture_);
        jacket_texture_ = {};
        jacket_loaded_ = false;
    }

    const song_entry* song = selected_song();
    if (song == nullptr) {
        return;
    }

    if (song->song.meta.jacket_file.empty()) {
        return;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song->song.directory, song->song.meta.jacket_file);
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        return;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    jacket_texture_ = LoadTexture(jacket_path_utf8.c_str());
    jacket_loaded_ = jacket_texture_.id != 0;
}

void song_select_scene::stop_preview_and_unload_jacket() {
    audio_manager::instance().stop_preview();
    preview_song_id_.clear();
    pending_preview_song_.reset();
    active_preview_song_.reset();
    preview_volume_ = 0.0f;
    preview_fade_direction_ = 0;
    if (jacket_loaded_) {
        UnloadTexture(jacket_texture_);
        jacket_texture_ = {};
        jacket_loaded_ = false;
    }
}

void song_select_scene::queue_preview_for_selected_song() {
    const song_entry* song = selected_song();
    if (song == nullptr) {
        return;
    }

    if (preview_song_id_ == song->song.meta.song_id) {
        return;
    }

    pending_preview_song_ = song->song;
    if (preview_song_id_.empty() || !audio_manager::instance().is_preview_loaded()) {
        start_preview(*song);
        return;
    }

    preview_fade_direction_ = -1;
}

void song_select_scene::start_preview(const song_entry& song) {
    audio_manager& audio = audio_manager::instance();
    const std::filesystem::path audio_path = path_utils::join_utf8(song.song.directory, song.song.meta.audio_file);
    audio.load_preview(path_utils::to_utf8(audio_path));
    if (!audio.is_preview_loaded()) {
        preview_song_id_.clear();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        return;
    }

    audio.seek_preview(song.song.meta.preview_start_seconds);
    audio.set_preview_volume(0.0f);
    audio.play_preview(false);
    preview_volume_ = 0.0f;
    preview_song_id_ = song.song.meta.song_id;
    active_preview_song_ = song.song;
    preview_fade_direction_ = 1;
    pending_preview_song_.reset();
}

void song_select_scene::update_preview(float dt) {
    if (preview_fade_direction_ < 0) {
        preview_volume_ = std::max(0.0f, preview_volume_ - dt * kPreviewFadeSpeed);
        audio_manager::instance().set_preview_volume(preview_volume_ * g_settings.bgm_volume);
        if (preview_volume_ <= 0.0f) {
            audio_manager::instance().stop_preview();
            preview_song_id_.clear();
            active_preview_song_.reset();
            preview_fade_direction_ = 0;
            if (pending_preview_song_.has_value()) {
                const song_entry* song = selected_song();
                if (song != nullptr && song->song.meta.song_id == pending_preview_song_->meta.song_id) {
                    start_preview(*song);
                } else {
                    pending_preview_song_.reset();
                }
            }
        }
    } else if (preview_fade_direction_ > 0) {
        preview_volume_ = std::min(kPreviewMaxVolume, preview_volume_ + dt * kPreviewFadeSpeed);
        audio_manager::instance().set_preview_volume(preview_volume_ * g_settings.bgm_volume);
        if (preview_volume_ >= kPreviewMaxVolume) {
            preview_fade_direction_ = 0;
        }
    } else if (active_preview_song_.has_value() && audio_manager::instance().is_preview_loaded()) {
        const double remaining = audio_manager::instance().get_preview_length_seconds() -
                                 audio_manager::instance().get_preview_position_seconds();
        if (remaining <= 1.0) {
            preview_fade_direction_ = -1;
            pending_preview_song_ = *active_preview_song_;
        }
    }
}

const song_select_scene::chart_option* song_select_scene::selected_chart_for(
    const std::vector<const chart_option*>& filtered) const {
    if (filtered.empty()) {
        return nullptr;
    }
    const int index = std::min<int>(difficulty_index_, static_cast<int>(filtered.size()) - 1);
    return filtered[static_cast<size_t>(index)];
}

void song_select_scene::apply_song_selection(int song_index, int chart_index) {
    if (songs_.empty()) {
        return;
    }

    const int clamped_song_index = std::clamp(song_index, 0, static_cast<int>(songs_.size()) - 1);
    const bool song_changed = clamped_song_index != selected_song_index_;
    selected_song_index_ = clamped_song_index;

    const auto filtered = filtered_charts_for_selected_song();
    if (filtered.empty()) {
        difficulty_index_ = 0;
    } else {
        difficulty_index_ = std::clamp(chart_index, 0, static_cast<int>(filtered.size()) - 1);
    }

    if (song_changed) {
        song_change_anim_t_ = 1.0f;
        queue_preview_for_selected_song();
        load_jacket_for_selected_song();
    }
}

void song_select_scene::open_song_context_menu(int song_index, Vector2 mouse) {
    context_menu_.open = true;
    context_menu_.target = context_menu_target::song;
    context_menu_.song_index = song_index;
    context_menu_.chart_index = -1;
    context_menu_.rect = make_context_menu_rect(mouse, 3);
}

void song_select_scene::open_chart_context_menu(int song_index, int chart_index, Vector2 mouse) {
    context_menu_.open = true;
    context_menu_.target = context_menu_target::chart;
    context_menu_.song_index = song_index;
    context_menu_.chart_index = chart_index;
    context_menu_.rect = make_context_menu_rect(mouse, 2);
}

void song_select_scene::close_context_menu() {
    context_menu_ = {};
}

void song_select_scene::queue_status_message(std::string message, bool is_error) {
    status_message_ = std::move(message);
    status_message_is_error_ = is_error;
}

std::string song_select_scene::fallback_song_id_after_song_delete(int song_index) const {
    if (songs_.size() <= 1) {
        return "";
    }
    if (song_index + 1 < static_cast<int>(songs_.size())) {
        return songs_[static_cast<size_t>(song_index + 1)].song.meta.song_id;
    }
    if (song_index > 0) {
        return songs_[static_cast<size_t>(song_index - 1)].song.meta.song_id;
    }
    return "";
}

std::string song_select_scene::fallback_chart_id_after_chart_delete(int song_index, int chart_index) const {
    if (song_index < 0 || song_index >= static_cast<int>(songs_.size())) {
        return "";
    }

    const auto& charts = songs_[static_cast<size_t>(song_index)].charts;
    if (charts.size() <= 1) {
        return "";
    }
    if (chart_index + 1 < static_cast<int>(charts.size())) {
        return charts[static_cast<size_t>(chart_index + 1)].meta.chart_id;
    }
    if (chart_index > 0) {
        return charts[static_cast<size_t>(chart_index - 1)].meta.chart_id;
    }
    return "";
}

bool song_select_scene::delete_song_content(int song_index) {
    if (song_index < 0 || song_index >= static_cast<int>(songs_.size())) {
        queue_status_message("Song delete target is invalid.", true);
        return false;
    }

    const song_entry& entry = songs_[static_cast<size_t>(song_index)];
    if (!entry.song.can_delete) {
        queue_status_message("Only AppData songs can be deleted.", true);
        return false;
    }

    const std::filesystem::path song_dir = path_utils::from_utf8(entry.song.directory);
    if (!is_within_root(song_dir, app_paths::songs_root())) {
        queue_status_message("Refused to delete a song outside AppData.", true);
        return false;
    }

    std::vector<std::filesystem::path> chart_paths_to_delete;
    const std::filesystem::path charts_root = app_paths::charts_root();
    if (std::filesystem::exists(charts_root) && std::filesystem::is_directory(charts_root)) {
        for (const auto& chart_entry : std::filesystem::directory_iterator(charts_root)) {
            if (!chart_entry.is_regular_file() || chart_entry.path().extension() != ".chart") {
                continue;
            }

            const chart_parse_result parse_result = song_loader::load_chart(path_utils::to_utf8(chart_entry.path()));
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            if (parse_result.data->meta.song_id == entry.song.meta.song_id) {
                chart_paths_to_delete.push_back(chart_entry.path());
            }
        }
    }

    std::error_code ec;
    for (const auto& chart_path : chart_paths_to_delete) {
        std::filesystem::remove(chart_path, ec);
        if (ec) {
            queue_status_message("Failed to delete a linked chart file.", true);
            return false;
        }
    }

    std::filesystem::remove_all(song_dir, ec);
    if (ec) {
        queue_status_message("Failed to delete the song directory.", true);
        return false;
    }

    const std::string fallback_song_id = fallback_song_id_after_song_delete(song_index);
    close_context_menu();
    confirmation_dialog_ = {};
    reload_song_library(fallback_song_id);
    queue_status_message("Song deleted.", false);
    return true;
}

bool song_select_scene::delete_chart_content(int song_index, int chart_index) {
    if (song_index < 0 || song_index >= static_cast<int>(songs_.size())) {
        queue_status_message("Chart delete target is invalid.", true);
        return false;
    }

    const auto& charts = songs_[static_cast<size_t>(song_index)].charts;
    if (chart_index < 0 || chart_index >= static_cast<int>(charts.size())) {
        queue_status_message("Chart delete target is invalid.", true);
        return false;
    }

    const chart_option& chart = charts[static_cast<size_t>(chart_index)];
    if (!chart.can_delete) {
        queue_status_message("Only AppData charts can be deleted.", true);
        return false;
    }

    const std::filesystem::path chart_path = path_utils::from_utf8(chart.path);
    if (!is_within_root(chart_path, app_paths::app_data_root())) {
        queue_status_message("Refused to delete a chart outside AppData.", true);
        return false;
    }

    std::error_code ec;
    const bool removed = std::filesystem::remove(chart_path, ec);
    if (ec || !removed) {
        queue_status_message("Failed to delete the chart file.", true);
        return false;
    }

    const std::string song_id = songs_[static_cast<size_t>(song_index)].song.meta.song_id;
    const std::string fallback_chart_id = fallback_chart_id_after_chart_delete(song_index, chart_index);
    close_context_menu();
    confirmation_dialog_ = {};
    reload_song_library(song_id, fallback_chart_id);
    queue_status_message("Chart deleted.", false);
    return true;
}

bool song_select_scene::handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed) {
    if (context_menu_.open && ui::is_hovered(context_menu_.rect, kContextMenuLayer)) {
        return false;
    }
    if (!CheckCollisionPointRec(mouse, kSongListViewRect) || (!left_pressed && !right_pressed)) {
        return false;
    }

    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    float item_y = kSongListViewRect.y - scroll_y_;
    for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
        float row_h = kRowHeight;
        if (i == selected_song_index_) {
            row_h = kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f;
            float child_y = item_y + 46.0f;
            for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
                const Rectangle child_rect = {kSongListRect.x + 46.0f, child_y - 6.0f, kSongListRect.width - 92.0f, 28.0f};
                if (CheckCollisionPointRec(mouse, child_rect)) {
                    if (right_pressed) {
                        apply_song_selection(i, chart_index);
                        open_chart_context_menu(i, chart_index, mouse);
                        return true;
                    }

                    close_context_menu();
                    if (difficulty_index_ == chart_index) {
                        manager_.change_scene(std::make_unique<play_scene>(manager_, selected_song()->song,
                                                                           filtered[static_cast<size_t>(chart_index)]->path,
                                                                           filtered[static_cast<size_t>(chart_index)]->meta.key_count));
                    } else {
                        apply_song_selection(i, chart_index);
                    }
                    return true;
                }
                child_y += 30.0f;
            }
        }

        const Rectangle row_rect = {kSongListRect.x + 14.0f, item_y - 8.0f, kSongListRect.width - 28.0f, 44.0f};
        if (CheckCollisionPointRec(mouse, row_rect)) {
            if (right_pressed) {
                const int chart_index = (i == selected_song_index_) ? difficulty_index_ : 0;
                apply_song_selection(i, chart_index);
                open_song_context_menu(i, mouse);
                return true;
            }

            close_context_menu();
            apply_song_selection(i, (i == selected_song_index_) ? difficulty_index_ : 0);
            return true;
        }

        item_y += row_h;
    }

    return false;
}

void song_select_scene::draw_song_details(const song_entry& song, const chart_option* selected_chart,
                                          float content_offset_x, unsigned char content_alpha) const {
    const auto& t = *g_theme;
    ui::draw_section(kJacketRect);
    if (jacket_loaded_) {
        const Rectangle source = {0.0f, 0.0f, static_cast<float>(jacket_texture_.width), static_cast<float>(jacket_texture_.height)};
        DrawTexturePro(jacket_texture_, source, kJacketRect, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, content_alpha});
    } else {
        ui::draw_text_in_rect("JACKET", 30, kJacketRect, with_alpha(t.text_muted, content_alpha));
    }
    DrawRectangleLinesEx(kJacketRect, 2.0f, t.border_image);

    const float detail_x = kJacketRect.x + kJacketRect.width + 20.0f;
    const float detail_max_width = kLeftPanelRect.x + kLeftPanelRect.width - detail_x - 16.0f;
    const double now = GetTime();
    draw_marquee_text(song.song.meta.title.c_str(), detail_x + content_offset_x, kJacketRect.y + 4.0f, 40,
                      with_alpha(t.text, content_alpha), detail_max_width, now);
    draw_marquee_text(song.song.meta.artist.c_str(), detail_x + content_offset_x, kJacketRect.y + 56.0f, 28,
                      with_alpha(t.text_secondary, content_alpha), detail_max_width, now);
    ui::draw_text_f(TextFormat("BPM %.0f", song.song.meta.base_bpm), detail_x + content_offset_x,
                    kJacketRect.y + 100.0f, 24, with_alpha(t.text_muted, content_alpha));
    ui::draw_text_f(TextFormat("Source %s", source_label(song.song.source)),
                    detail_x + content_offset_x, kJacketRect.y + 126.0f, 18,
                    with_alpha(song.song.source == content_source::app_data ? t.success : t.text_hint, content_alpha));
    if (selected_chart != nullptr) {
        ui::draw_text_f(TextFormat("%s %s Lv.%d", key_mode_label(selected_chart->meta.key_count).c_str(),
                                   selected_chart->meta.difficulty.c_str(), selected_chart->meta.level),
                        detail_x + content_offset_x, kJacketRect.y + 158.0f, 28, with_alpha(t.text, content_alpha));
        ui::draw_text_f(selected_chart->meta.chart_author.c_str(), detail_x + content_offset_x,
                        kJacketRect.y + 194.0f, 20, with_alpha(t.text_muted, content_alpha));
    }
}

void song_select_scene::draw_song_row(const song_entry& song, float item_y, bool is_selected, double now) const {
    const auto& t = *g_theme;
    const Rectangle row_rect = {kSongListRect.x + 14.0f, item_y - 8.0f, kSongListRect.width - 28.0f, 44.0f};
    const float text_x = kSongListRect.x + 30.0f;
    const float list_text_max_w = kSongListRect.width - 70.0f;
    const Rectangle title_clip_rect = {text_x, item_y, list_text_max_w, 24.0f};
    const Rectangle artist_clip_rect = {text_x, item_y + 22.0f, list_text_max_w, 16.0f};

    if (ui::is_hovered(row_rect, kSongSelectLayer) || is_selected) {
        const ui::row_state row_state = ui::draw_selectable_row(row_rect, is_selected, 0.0f);
        (void)row_state;
    }

    draw_marquee_text(song.song.meta.title.c_str(), title_clip_rect,
                      24, is_selected ? t.text : t.text_secondary, now);
    draw_marquee_text(song.song.meta.artist.c_str(), artist_clip_rect,
                      16, t.text_muted, now);
}

void song_select_scene::draw_chart_rows(const std::vector<const chart_option*>& filtered, float item_y) const {
    const auto& t = *g_theme;
    const float child_x = kSongListRect.x + 46.0f;
    const float child_w = kSongListRect.width - 92.0f;
    const float child_text_x = kSongListRect.x + 58.0f;
    const float author_x = kSongListRect.x + kSongListRect.width - 120.0f;
    float child_y = item_y + 46.0f;
    for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
        const chart_option& chart = *filtered[static_cast<size_t>(chart_index)];
        const bool child_selected = chart_index == difficulty_index_;
        const Rectangle child_rect = {child_x, child_y - 6.0f, child_w, 28.0f};
        if (ui::is_hovered(child_rect, kSongSelectLayer) || child_selected) {
            const ui::row_state child_state = ui::draw_selectable_row(child_rect, child_selected, 0.0f);
            (void)child_state;
        }
        ui::draw_text_f(TextFormat("%s %s Lv.%d", key_mode_label(chart.meta.key_count).c_str(), chart.meta.difficulty.c_str(),
                                   chart.meta.level),
                        child_text_x, child_y, 18, child_selected ? t.text : t.text_secondary);
        ui::draw_text_f(chart.meta.chart_author.c_str(), author_x, child_y + 1.0f, 14, t.text_muted);
        child_y += 30.0f;
    }
}

void song_select_scene::draw_song_list(const std::vector<const chart_option*>& filtered) const {
    const auto& t = *g_theme;
    ui::draw_text_in_rect("Songs", 28, kSongListTitleRect, t.text, ui::text_align::left);
    ui::draw_button_colored(kSongListNewSongButtonRect, "NEW SONG", 14, t.row, t.row_hover, t.text);

    ui::scoped_clip_rect clip_scope(kSongListViewRect);

    const double now = GetTime();
    float item_y = kSongListViewRect.y - scroll_y_;
    for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
        const bool is_selected = i == selected_song_index_;
        float row_h = is_selected
            ? kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f
            : kRowHeight;

        if (item_y + row_h < kSongListViewRect.y) {
            item_y += row_h;
            continue;
        }
        if (item_y > kSongListViewRect.y + kSongListViewRect.height) {
            break;
        }

        draw_song_row(songs_[static_cast<size_t>(i)], item_y, is_selected, now);
        if (is_selected) {
            draw_chart_rows(filtered, item_y);
        }
        item_y += row_h;
    }
    ui::draw_scrollbar(kSongListScrollbarTrackRect, compute_content_height(), scroll_y_, t.scrollbar_track, t.scrollbar_thumb);
}

// 全曲リストの合計高さを返す（選択曲の展開分を含む）。
float song_select_scene::compute_content_height() const {
    float total = 0.0f;
    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
        if (i == selected_song_index_) {
            total += kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f;
        } else {
            total += kRowHeight;
        }
    }
    return total;
}

void song_select_scene::draw_context_menu() {
    if (!context_menu_.open) {
        return;
    }

    std::vector<ui::context_menu_item> items;
    if (context_menu_.target == context_menu_target::song) {
        const bool valid_song = context_menu_.song_index >= 0 && context_menu_.song_index < static_cast<int>(songs_.size());
        const bool can_edit_song = valid_song && songs_[static_cast<size_t>(context_menu_.song_index)].song.can_edit;
        const bool can_delete_song = valid_song && songs_[static_cast<size_t>(context_menu_.song_index)].song.can_delete;
        items = {
            {"EDIT META", can_edit_song},
            {"NEW CHART", valid_song},
            {"DELETE SONG", can_delete_song},
        };
    } else {
        bool can_delete_chart = false;
        if (context_menu_.song_index >= 0 && context_menu_.song_index < static_cast<int>(songs_.size())) {
            const auto& charts = songs_[static_cast<size_t>(context_menu_.song_index)].charts;
            if (context_menu_.chart_index >= 0 && context_menu_.chart_index < static_cast<int>(charts.size())) {
                can_delete_chart = charts[static_cast<size_t>(context_menu_.chart_index)].can_delete;
            }
        }
        items = {
            {"EDIT CHART", true},
            {"DELETE CHART", can_delete_chart},
        };
    }

    const ui::context_menu_state menu = ui::enqueue_context_menu(context_menu_.rect, items, kContextMenuLayer, 16,
                                                                 kContextMenuItemHeight, kContextMenuItemSpacing);
    if (menu.clicked_index == 0) {
        if (context_menu_.target == context_menu_target::song &&
            context_menu_.song_index >= 0 && context_menu_.song_index < static_cast<int>(songs_.size()) &&
            songs_[static_cast<size_t>(context_menu_.song_index)].song.can_edit) {
            const song_data song = songs_[static_cast<size_t>(context_menu_.song_index)].song;
            close_context_menu();
            manager_.change_scene(std::make_unique<song_create_scene>(manager_, song));
            return;
        }

        if (context_menu_.target == context_menu_target::chart &&
            context_menu_.song_index >= 0 && context_menu_.song_index < static_cast<int>(songs_.size())) {
            const auto& song = songs_[static_cast<size_t>(context_menu_.song_index)].song;
            const auto& charts = songs_[static_cast<size_t>(context_menu_.song_index)].charts;
            if (context_menu_.chart_index >= 0 && context_menu_.chart_index < static_cast<int>(charts.size())) {
                const std::string chart_path = charts[static_cast<size_t>(context_menu_.chart_index)].path;
                close_context_menu();
                manager_.change_scene(std::make_unique<editor_scene>(manager_, song, chart_path));
                return;
            }
        }
    } else if (context_menu_.target == context_menu_target::song && menu.clicked_index == 1) {
        if (context_menu_.song_index >= 0 && context_menu_.song_index < static_cast<int>(songs_.size())) {
            const song_entry& entry = songs_[static_cast<size_t>(context_menu_.song_index)];
            const int key_count = entry.charts.empty()
                ? 4
                : entry.charts[static_cast<size_t>(std::clamp(difficulty_index_, 0, static_cast<int>(entry.charts.size()) - 1))].meta.key_count;
            const song_data song = entry.song;
            close_context_menu();
            manager_.change_scene(std::make_unique<editor_scene>(manager_, song, key_count));
            return;
        }
    } else if ((context_menu_.target == context_menu_target::song && menu.clicked_index == 2) ||
               (context_menu_.target == context_menu_target::chart && menu.clicked_index == 1)) {
        confirmation_dialog_.open = true;
        confirmation_dialog_.action = context_menu_.target == context_menu_target::song
            ? pending_confirmation_action::delete_song
            : pending_confirmation_action::delete_chart;
        confirmation_dialog_.song_index = context_menu_.song_index;
        confirmation_dialog_.chart_index = context_menu_.chart_index;
        close_context_menu();
        return;
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
               !ui::is_hovered(context_menu_.rect, kContextMenuLayer)) {
        close_context_menu();
    }
}

void song_select_scene::draw_confirmation_dialog() {
    if (!confirmation_dialog_.open) {
        return;
    }

    const bool deleting_song = confirmation_dialog_.action == pending_confirmation_action::delete_song;
    const char* title = deleting_song ? "Delete Song" : "Delete Chart";
    const char* message = deleting_song
        ? "This will remove the song and linked AppData charts."
        : "This will remove the selected AppData chart file.";
    const Rectangle confirm_button = {kConfirmDialogRect.x + 76.0f, kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};
    const Rectangle cancel_button = {kConfirmDialogRect.x + 272.0f, kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kConfirmDialogRect, kModalLayer);
    ui::enqueue_text_in_rect(title, 28,
                             {kConfirmDialogRect.x + 20.0f, kConfirmDialogRect.y + 22.0f,
                              kConfirmDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, kModalLayer);
    ui::enqueue_text_in_rect(message, 18,
                             {kConfirmDialogRect.x + 28.0f, kConfirmDialogRect.y + 76.0f,
                              kConfirmDialogRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, kModalLayer);
    ui::enqueue_text_in_rect("Legacy assets are not delete targets.", 16,
                             {kConfirmDialogRect.x + 28.0f, kConfirmDialogRect.y + 104.0f,
                              kConfirmDialogRect.width - 56.0f, 22.0f},
                             g_theme->text_hint, ui::text_align::center, kModalLayer);
    const ui::button_state confirm = ui::enqueue_button(confirm_button, "DELETE", 16, kModalLayer, 1.5f);
    const ui::button_state cancel = ui::enqueue_button(cancel_button, "CANCEL", 16, kModalLayer, 1.5f);

    if (confirm.clicked) {
        if (deleting_song) {
            delete_song_content(confirmation_dialog_.song_index);
        } else {
            delete_chart_content(confirmation_dialog_.song_index, confirmation_dialog_.chart_index);
        }
    } else if (cancel.clicked || (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                                  !ui::is_hovered(kConfirmDialogRect, kModalLayer))) {
        confirmation_dialog_ = {};
    }
}

void song_select_scene::update(float dt) {
    ui::begin_hit_regions();
    if (context_menu_.open) {
        ui::register_hit_region(context_menu_.rect, kContextMenuLayer);
    }
    if (confirmation_dialog_.open) {
        ui::register_hit_region(kConfirmDialogRect, kModalLayer);
    }

    update_preview(dt);
    song_change_anim_t_ = std::max(0.0f, song_change_anim_t_ - dt * 4.0f);
    scene_fade_in_t_ = std::max(0.0f, scene_fade_in_t_ - dt / 0.3f);

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (confirmation_dialog_.open) {
            confirmation_dialog_ = {};
            return;
        }
        if (context_menu_.open) {
            close_context_menu();
            return;
        }
        manager_.change_scene(std::make_unique<title_scene>(manager_));
        return;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();

    if (confirmation_dialog_.open) {
        return;
    }

    if (context_menu_.open && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
        !ui::is_hovered(context_menu_.rect, kContextMenuLayer) &&
        !CheckCollisionPointRec(mouse, kSongListViewRect)) {
        close_context_menu();
        return;
    }

    if (IsKeyPressed(KEY_F1) ||
        ui::is_clicked(kSettingsButtonRect, kSongSelectLayer)) {
        close_context_menu();
        manager_.change_scene(std::make_unique<settings_scene>(manager_, settings_scene::return_target::song_select));
        return;
    }

    if (ui::is_clicked(kSongListNewSongButtonRect, kSongSelectLayer)) {
        close_context_menu();
        manager_.change_scene(std::make_unique<song_create_scene>(manager_));
        return;
    }

    if (songs_.empty()) {
        return;
    }

    const bool song_list_hovered = ui::is_hovered(kSongListViewRect, kSongSelectLayer);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        close_context_menu();
        apply_song_selection(std::max(0, selected_song_index_ - 1), 0);
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        close_context_menu();
        apply_song_selection(std::min(static_cast<int>(songs_.size()) - 1, selected_song_index_ + 1), 0);
    }

    const float content_height = compute_content_height();
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        kSongListScrollbarTrackRect, content_height, scroll_y_target_, scrollbar_dragging_, scrollbar_drag_offset_,
        kSongSelectLayer);
    scroll_y_target_ = scrollbar.scroll_offset;
    if (scrollbar.changed || scrollbar.dragging) {
        scroll_y_ = scroll_y_target_;
    }

    if (!scrollbar.dragging && song_list_hovered && wheel != 0.0f) {
        scroll_y_target_ -= wheel * kScrollWheelStep;
    }

    const float max_scroll = ui::vertical_scroll_metrics(kSongListScrollbarTrackRect, content_height, scroll_y_target_).max_scroll;
    scroll_y_target_ = std::clamp(scroll_y_target_, 0.0f, max_scroll);
    scroll_y_ += (scroll_y_target_ - scroll_y_) * std::min(1.0f, kScrollLerpSpeed * dt);
    if (std::fabs(scroll_y_ - scroll_y_target_) < 0.5f) {
        scroll_y_ = scroll_y_target_;
    }

    std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    if (!filtered.empty()) {
        difficulty_index_ = std::clamp(difficulty_index_, 0, static_cast<int>(filtered.size()) - 1);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            close_context_menu();
            difficulty_index_ = std::max(0, difficulty_index_ - 1);
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            close_context_menu();
            difficulty_index_ = std::min(static_cast<int>(filtered.size()) - 1, difficulty_index_ + 1);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            close_context_menu();
            manager_.change_scene(std::make_unique<play_scene>(manager_, selected_song()->song,
                                                               filtered[static_cast<size_t>(difficulty_index_)]->path,
                                                               filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count));
            return;
        }

        if (IsKeyPressed(KEY_E)) {
            close_context_menu();
            manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song,
                                                                 filtered[static_cast<size_t>(difficulty_index_)]->path));
            return;
        }

        if (IsKeyPressed(KEY_N)) {
            close_context_menu();
            manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song,
                                                                 filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count));
            return;
        }
    }

    if (handle_song_list_pointer(mouse, IsMouseButtonPressed(MOUSE_BUTTON_LEFT), IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        return;
    }
}

void song_select_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::draw_panel(kLeftPanelRect);
    ui::draw_panel(kSongListRect);
    ui::begin_draw_queue();
    ui::draw_text_in_rect("SONG SELECT", 30, kSceneTitleRect, t.text, ui::text_align::left);
    ui::draw_button_colored(kSettingsButtonRect, "SETTINGS", 20,
                            t.row, t.row_hover, t.text);

    if (songs_.empty()) {
        ui::draw_text_in_rect("No songs found", 36,
                              ui::place(kLeftPanelRect, 320.0f, 40.0f,
                                        ui::anchor::center, ui::anchor::center,
                                        {0.0f, -20.0f}),
                              t.text);
        if (!load_errors_.empty()) {
            ui::draw_text_in_rect(load_errors_.front().c_str(), 22,
                                  ui::place(kLeftPanelRect, 620.0f, 28.0f,
                                            ui::anchor::center, ui::anchor::center,
                                            {0.0f, 28.0f}),
                                  t.error);
        }
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    const song_entry& song = songs_[static_cast<size_t>(selected_song_index_)];
    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    const chart_option* selected_chart = selected_chart_for(filtered);
    const float content_anim = 1.0f - song_change_anim_t_;
    const float content_offset_x = 18.0f * song_change_anim_t_;
    const unsigned char content_alpha = static_cast<unsigned char>(145.0f + 110.0f * content_anim);

    draw_song_details(song, selected_chart, content_offset_x, content_alpha);
    draw_song_list(filtered);
    if (!status_message_.empty()) {
        ui::draw_text_in_rect(status_message_.c_str(), 18,
                              ui::place(kScreenRect, 520.0f, 24.0f,
                                        ui::anchor::bottom_right, ui::anchor::bottom_right,
                                        {-24.0f, -10.0f}),
                              status_message_is_error_ ? t.error : t.success, ui::text_align::right);
    }

    draw_context_menu();
    draw_confirmation_dialog();
    ui::flush_draw_queue();

    if (scene_fade_in_t_ > 0.0f) {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      Color{0, 0, 0, static_cast<unsigned char>(scene_fade_in_t_ * 0.65f * 255.0f)});
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
