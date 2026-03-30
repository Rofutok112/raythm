#include "song_select_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

#include "editor_scene.h"
#include "game_settings.h"
#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_scene.h"
#include "song_loader.h"
#include "theme.h"
#include "title_scene.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
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
constexpr Rectangle kActionPanelRect = ui::place(kLeftPanelRect, 710.0f, 72.0f,
                                                 ui::anchor::bottom_center, ui::anchor::bottom_center,
                                                 {0.0f, -18.0f});
constexpr Rectangle kPlayButtonRect = {kActionPanelRect.x + 12.0f, kActionPanelRect.y + 26.0f, 214.0f, 34.0f};
constexpr Rectangle kEditButtonRect = {kActionPanelRect.x + 248.0f, kActionPanelRect.y + 26.0f, 214.0f, 34.0f};
constexpr Rectangle kNewChartButtonRect = {kActionPanelRect.x + 484.0f, kActionPanelRect.y + 26.0f, 214.0f, 34.0f};
constexpr Rectangle kSongListTitleRect = ui::place(kSongListRect, 180.0f, 28.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {20.0f, 10.0f});
constexpr float kSongListHeaderHeight = 48.0f;
constexpr float kSongListBottomPadding = 12.0f;
constexpr Rectangle kSongListViewRect = ui::scroll_view(kSongListRect, kSongListHeaderHeight, kSongListBottomPadding);
constexpr Rectangle kSongListScrollbarTrackRect = ui::place(kSongListViewRect, 6.0f, kSongListViewRect.height,
                                                            ui::anchor::top_right, ui::anchor::top_right,
                                                            {-8.0f, 0.0f});
constexpr float kPreviewFadeSpeed = 2.4f;
constexpr float kPreviewMaxVolume = 0.55f;

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

}

song_select_scene::song_select_scene(scene_manager& manager) : scene(manager) {
}

void song_select_scene::on_enter() {
    songs_.clear();
    load_errors_.clear();
    selected_song_index_ = 0;
    difficulty_index_ = 0;
    scroll_y_ = 0.0f;
    scroll_y_target_ = 0.0f;
    song_change_anim_t_ = 1.0f;
    scene_fade_in_t_ = 1.0f;

    const song_load_result load_result = song_loader::load_all((repo_root() / "assets" / "songs").string());
    load_errors_ = load_result.errors;

    for (const song_data& song : load_result.songs) {
        song_entry entry;
        entry.song = song;

        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            entry.charts.push_back({chart_path, parse_result.data->meta});
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

        if (!entry.charts.empty()) {
            songs_.push_back(std::move(entry));
        }
    }

    queue_preview_for_selected_song();
    load_jacket_for_selected_song();
}

void song_select_scene::on_exit() {
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

    const std::filesystem::path jacket_path = std::filesystem::path(song->song.directory) / song->song.meta.jacket_file;
    if (!std::filesystem::exists(jacket_path)) {
        return;
    }

    jacket_texture_ = LoadTexture(jacket_path.string().c_str());
    jacket_loaded_ = jacket_texture_.id != 0;
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
    const std::filesystem::path audio_path = std::filesystem::path(song.song.directory) / song.song.meta.audio_file;
    audio.load_preview(audio_path.string());
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
    draw_marquee_text(song.song.meta.title.c_str(), static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 4.0f), 40,
                      with_alpha(t.text, content_alpha), detail_max_width, now);
    draw_marquee_text(song.song.meta.artist.c_str(), static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 56.0f), 28,
                      with_alpha(t.text_secondary, content_alpha), detail_max_width, now);
    DrawText(TextFormat("BPM %.0f", song.song.meta.base_bpm), static_cast<int>(detail_x + content_offset_x),
             static_cast<int>(kJacketRect.y + 100.0f), 24, with_alpha(t.text_muted, content_alpha));
    if (selected_chart != nullptr) {
        DrawText(TextFormat("%s %s Lv.%d", key_mode_label(selected_chart->meta.key_count).c_str(),
                            selected_chart->meta.difficulty.c_str(), selected_chart->meta.level),
                 static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 150.0f), 28, with_alpha(t.text, content_alpha));
        DrawText(selected_chart->meta.chart_author.c_str(), static_cast<int>(detail_x + content_offset_x),
                 static_cast<int>(kJacketRect.y + 186.0f), 20, with_alpha(t.text_muted, content_alpha));
    }

    ui::draw_section(kActionPanelRect);
    ui::draw_text_in_rect("Actions", 18,
                          {kActionPanelRect.x + 14.0f, kActionPanelRect.y + 4.0f, 120.0f, 18.0f},
                          t.text_hint, ui::text_align::left);
    ui::draw_button_colored(kPlayButtonRect, "PLAY", 20, t.row_selected, t.row_active, t.text);
    ui::draw_button_colored(kEditButtonRect, "EDIT", 20, t.row, t.row_hover, t.text);
    ui::draw_button_colored(kNewChartButtonRect, "NEW", 20, t.row, t.row_hover, t.text);
}

void song_select_scene::draw_song_row(const song_entry& song, float item_y, bool is_selected, double now) const {
    const auto& t = *g_theme;
    const int iy = static_cast<int>(item_y);
    const Rectangle row_rect = {kSongListRect.x + 14.0f, item_y - 8.0f, kSongListRect.width - 28.0f, 44.0f};
    const int text_x = static_cast<int>(kSongListRect.x + 30.0f);
    const float list_text_max_w = kSongListRect.width - 70.0f;

    if (ui::is_hovered(row_rect) || is_selected) {
        const ui::row_state row_state = ui::draw_selectable_row(row_rect, is_selected, 0.0f);
        (void)row_state;
    }

    draw_marquee_text(song.song.meta.title.c_str(), text_x, iy, 24,
                      is_selected ? t.text : t.text_secondary, list_text_max_w, now);
    draw_marquee_text(song.song.meta.artist.c_str(), text_x, iy + 22, 16,
                      t.text_muted, list_text_max_w, now);
}

void song_select_scene::draw_chart_rows(const std::vector<const chart_option*>& filtered, float item_y) const {
    const auto& t = *g_theme;
    const float child_x = kSongListRect.x + 46.0f;
    const float child_w = kSongListRect.width - 92.0f;
    const int child_text_x = static_cast<int>(kSongListRect.x + 58.0f);
    const int author_x = static_cast<int>(kSongListRect.x + kSongListRect.width - 120.0f);
    float child_y = item_y + 46.0f;
    for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
        const chart_option& chart = *filtered[static_cast<size_t>(chart_index)];
        const bool child_selected = chart_index == difficulty_index_;
        const Rectangle child_rect = {child_x, child_y - 6.0f, child_w, 28.0f};
        if (ui::is_hovered(child_rect) || child_selected) {
            const ui::row_state child_state = ui::draw_selectable_row(child_rect, child_selected, 0.0f);
            (void)child_state;
        }
        DrawText(TextFormat("%s %s Lv.%d", key_mode_label(chart.meta.key_count).c_str(), chart.meta.difficulty.c_str(),
                            chart.meta.level),
                 child_text_x, static_cast<int>(child_y), 18,
                 child_selected ? t.text : t.text_secondary);
        DrawText(chart.meta.chart_author.c_str(), author_x, static_cast<int>(child_y) + 1, 14, t.text_muted);
        child_y += 30.0f;
    }
}

void song_select_scene::draw_song_list(const std::vector<const chart_option*>& filtered) const {
    const auto& t = *g_theme;
    ui::draw_text_in_rect("Songs", 28, kSongListTitleRect, t.text, ui::text_align::left);

    BeginScissorMode(static_cast<int>(kSongListViewRect.x), static_cast<int>(kSongListViewRect.y),
                     static_cast<int>(kSongListViewRect.width), static_cast<int>(kSongListViewRect.height));

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
    EndScissorMode();

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

void song_select_scene::update(float dt) {
    update_preview(dt);

    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
        return;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();
    if (IsKeyPressed(KEY_F1) ||
        ui::is_clicked(kSettingsButtonRect)) {
        manager_.change_scene(std::make_unique<settings_scene>(manager_, settings_scene::return_target::song_select));
        return;
    }

    song_change_anim_t_ = std::max(0.0f, song_change_anim_t_ - dt * 4.0f);
    scene_fade_in_t_ = std::max(0.0f, scene_fade_in_t_ - dt / 0.3f);

    if (songs_.empty()) {
        return;
    }

    const int previous_song_index = selected_song_index_;

    // キー入力で曲選択
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_song_index_ = std::max(0, selected_song_index_ - 1);
        difficulty_index_ = 0;
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_song_index_ = std::min(static_cast<int>(songs_.size()) - 1, selected_song_index_ + 1);
        difficulty_index_ = 0;
    }

    const float content_height = compute_content_height();
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        kSongListScrollbarTrackRect, content_height, scroll_y_target_, scrollbar_dragging_, scrollbar_drag_offset_);
    scroll_y_target_ = scrollbar.scroll_offset;
    if (scrollbar.changed || scrollbar.dragging) {
        scroll_y_ = scroll_y_target_;
    }

    // マウスホイールでスムーズスクロール
    if (!scrollbar.dragging && CheckCollisionPointRec(mouse, kSongListViewRect) && wheel != 0.0f) {
        scroll_y_target_ -= wheel * kScrollWheelStep;
    }

    // スクロール目標値をコンテンツ範囲内にクランプ
    const float max_scroll = ui::vertical_scroll_metrics(kSongListScrollbarTrackRect, content_height, scroll_y_target_).max_scroll;
    scroll_y_target_ = std::clamp(scroll_y_target_, 0.0f, max_scroll);

    // 現在値を目標値に向けて補間（指数減衰）
    scroll_y_ += (scroll_y_target_ - scroll_y_) * std::min(1.0f, kScrollLerpSpeed * dt);
    if (std::fabs(scroll_y_ - scroll_y_target_) < 0.5f) {
        scroll_y_ = scroll_y_target_;
    }

    std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    if (!filtered.empty()) {
        difficulty_index_ = std::clamp(difficulty_index_, 0, static_cast<int>(filtered.size()) - 1);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            difficulty_index_ = std::max(0, difficulty_index_ - 1);
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            difficulty_index_ = std::min(static_cast<int>(filtered.size()) - 1, difficulty_index_ + 1);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            manager_.change_scene(std::make_unique<play_scene>(manager_, selected_song()->song,
                                                               filtered[static_cast<size_t>(difficulty_index_)]->path,
                                                               filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count));
            return;
        }

        if (IsKeyPressed(KEY_E)) {
            manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song,
                                                                 filtered[static_cast<size_t>(difficulty_index_)]->path));
            return;
        }

        if (IsKeyPressed(KEY_N)) {
            manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song,
                                                                 filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count));
            return;
        }
    }

    if (ui::is_clicked(kPlayButtonRect) && !filtered.empty()) {
        manager_.change_scene(std::make_unique<play_scene>(manager_, selected_song()->song,
                                                           filtered[static_cast<size_t>(difficulty_index_)]->path,
                                                           filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count));
        return;
    }

    if (ui::is_clicked(kEditButtonRect) && !filtered.empty()) {
        manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song,
                                                             filtered[static_cast<size_t>(difficulty_index_)]->path));
        return;
    }

    if (ui::is_clicked(kNewChartButtonRect)) {
        const int key_count = filtered.empty() ? 4 : filtered[static_cast<size_t>(difficulty_index_)]->meta.key_count;
        manager_.change_scene(std::make_unique<editor_scene>(manager_, selected_song()->song, key_count));
        return;
    }

    // リスト内クリック: スクロールオフセットを考慮して当たり判定
    if (CheckCollisionPointRec(mouse, kSongListViewRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        float item_y = kSongListViewRect.y - scroll_y_;
        for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
            float row_h = kRowHeight;
            if (i == selected_song_index_) {
                row_h = kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f;

                // 子要素（譜面）のクリック判定
                float child_y = item_y + 46.0f;
                for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
                    const Rectangle child_rect = {kSongListRect.x + 46.0f, child_y - 6.0f, kSongListRect.width - 92.0f, 28.0f};
                    if (child_rect.y >= kSongListViewRect.y && child_rect.y + child_rect.height <= kSongListViewRect.y + kSongListViewRect.height &&
                        CheckCollisionPointRec(mouse, child_rect)) {
                        if (difficulty_index_ == chart_index) {
                            // 選択中の難易度を再クリックでプレイ開始
                            manager_.change_scene(std::make_unique<play_scene>(manager_, selected_song()->song,
                                                                               filtered[static_cast<size_t>(chart_index)]->path,
                                                                               filtered[static_cast<size_t>(chart_index)]->meta.key_count));
                        } else {
                            difficulty_index_ = chart_index;
                        }
                        return;
                    }
                    child_y += 30.0f;
                }
            }

            const Rectangle row_rect = {kSongListRect.x + 14.0f, item_y - 8.0f, kSongListRect.width - 28.0f, 44.0f};
            if (row_rect.y >= kSongListViewRect.y && row_rect.y + row_rect.height <= kSongListViewRect.y + kSongListViewRect.height &&
                CheckCollisionPointRec(mouse, row_rect)) {
                if (selected_song_index_ != i) {
                    selected_song_index_ = i;
                    difficulty_index_ = 0;
                }
                break;
            }
            item_y += row_h;
        }
    }

    if (selected_song_index_ != previous_song_index) {
        song_change_anim_t_ = 1.0f;
        queue_preview_for_selected_song();
        load_jacket_for_selected_song();
    }
}

void song_select_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::draw_panel(kLeftPanelRect);
    ui::draw_panel(kSongListRect);
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
    ui::draw_text_in_rect("ENTER/PLAY: Start   E/EDIT: Open Editor   N/NEW: Blank Chart", 18,
                          ui::place(kScreenRect, 780.0f, 24.0f, ui::anchor::bottom_left, ui::anchor::bottom_left,
                                    {24.0f, -10.0f}),
                          t.text_hint, ui::text_align::left);

    if (scene_fade_in_t_ > 0.0f) {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      Color{0, 0, 0, static_cast<unsigned char>(scene_fade_in_t_ * 0.65f * 255.0f)});
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
