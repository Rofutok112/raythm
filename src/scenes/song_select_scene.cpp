#include "song_select_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

#include "game_settings.h"
#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_loader.h"
#include "title_scene.h"
#include "virtual_screen.h"

namespace {
constexpr float kRowHeight = 60.0f;
constexpr float kScrollWheelStep = 80.0f;
constexpr float kScrollLerpSpeed = 12.0f;
constexpr Rectangle kSongListRect = {790.0f, 44.0f, 466.0f, 660.0f};
constexpr Rectangle kLeftPanelRect = {24.0f, 44.0f, 750.0f, 660.0f};
constexpr Rectangle kJacketRect = {44.0f, 68.0f, 320.0f, 320.0f};
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
    preview_audio_.stop();
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
    if (preview_song_id_.empty() || !preview_audio_.is_loaded()) {
        start_preview(*song);
        return;
    }

    preview_fade_direction_ = -1;
}

void song_select_scene::start_preview(const song_entry& song) {
    const std::filesystem::path audio_path = std::filesystem::path(song.song.directory) / song.song.meta.audio_file;
    preview_audio_.load(audio_path.string());
    if (!preview_audio_.is_loaded()) {
        preview_song_id_.clear();
        preview_volume_ = 0.0f;
        preview_fade_direction_ = 0;
        return;
    }

    preview_audio_.set_position_seconds(song.song.meta.preview_start_seconds);
    preview_audio_.set_volume(0.0f);
    preview_audio_.play(false);
    preview_volume_ = 0.0f;
    preview_song_id_ = song.song.meta.song_id;
    active_preview_song_ = song.song;
    preview_fade_direction_ = 1;
    pending_preview_song_.reset();
}

void song_select_scene::update_preview(float dt) {
    if (preview_fade_direction_ < 0) {
        preview_volume_ = std::max(0.0f, preview_volume_ - dt * kPreviewFadeSpeed);
        preview_audio_.set_volume(preview_volume_);
        if (preview_volume_ <= 0.0f) {
            preview_audio_.stop();
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
        preview_audio_.set_volume(preview_volume_);
        if (preview_volume_ >= kPreviewMaxVolume) {
            preview_fade_direction_ = 0;
        }
    } else if (active_preview_song_.has_value() && preview_audio_.is_loaded()) {
        const double remaining = preview_audio_.get_length_seconds() - preview_audio_.get_position_seconds();
        if (remaining <= 1.0) {
            preview_fade_direction_ = -1;
            pending_preview_song_ = *active_preview_song_;
        }
    }
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
    song_change_anim_t_ = std::max(0.0f, song_change_anim_t_ - dt * 4.0f);

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

    // マウスホイールでスムーズスクロール
    if (CheckCollisionPointRec(mouse, kSongListRect) && wheel != 0.0f) {
        scroll_y_target_ -= wheel * kScrollWheelStep;
    }

    // スクロール目標値をコンテンツ範囲内にクランプ
    const float list_view_height = kSongListRect.height - 72.0f;  // 60px header + 12px bottom padding
    const float content_height = compute_content_height();
    const float max_scroll = std::max(0.0f, content_height - list_view_height);
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
        }
    }

    // リスト内クリック: スクロールオフセットを考慮して当たり判定
    if (CheckCollisionPointRec(mouse, kSongListRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const float list_top = kSongListRect.y + 60.0f;
        float item_y = list_top - scroll_y_;
        for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
            float row_h = kRowHeight;
            if (i == selected_song_index_) {
                row_h = kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f;

                // 子要素（譜面）のクリック判定
                float child_y = item_y + 46.0f;
                for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
                    const Rectangle child_rect = {kSongListRect.x + 46.0f, child_y - 6.0f, kSongListRect.width - 92.0f, 28.0f};
                    if (child_rect.y >= kSongListRect.y && child_rect.y + child_rect.height <= kSongListRect.y + kSongListRect.height &&
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
            if (row_rect.y >= kSongListRect.y && row_rect.y + row_rect.height <= kSongListRect.y + kSongListRect.height &&
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
    virtual_screen::begin();
    ClearBackground(RAYWHITE);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {255, 255, 255, 255}, {241, 243, 246, 255});
    DrawRectangleRec(kLeftPanelRect, Color{248, 249, 251, 255});
    DrawRectangleRec(kSongListRect, Color{248, 249, 251, 255});
    DrawRectangleLinesEx(kLeftPanelRect, 2.0f, Color{206, 210, 218, 255});
    DrawRectangleLinesEx(kSongListRect, 2.0f, Color{206, 210, 218, 255});
    DrawText("SONG SELECT", 30, 12, 30, BLACK);

    if (songs_.empty()) {
        DrawText("No songs found", 50, 300, 36, BLACK);
        if (!load_errors_.empty()) {
            DrawText(load_errors_.front().c_str(), 50, 350, 22, Color{220, 38, 38, 255});
        }
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    const song_entry& song = songs_[static_cast<size_t>(selected_song_index_)];
    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song();
    const chart_option* selected_chart =
        filtered.empty() ? nullptr : filtered[static_cast<size_t>(std::min<int>(difficulty_index_, static_cast<int>(filtered.size()) - 1))];
    const Color panel_border = Color{206, 210, 218, 255};
    const Color song_list_fill = Color{248, 249, 251, 255};
    const float content_anim = 1.0f - song_change_anim_t_;
    const float content_offset_x = 18.0f * song_change_anim_t_;
    const unsigned char content_alpha = static_cast<unsigned char>(145.0f + 110.0f * content_anim);

    DrawRectangleRec(kJacketRect, Color{228, 231, 236, 255});
    if (jacket_loaded_) {
        const Rectangle source = {0.0f, 0.0f, static_cast<float>(jacket_texture_.width), static_cast<float>(jacket_texture_.height)};
        DrawTexturePro(jacket_texture_, source, kJacketRect, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, content_alpha});
    } else {
        const int jacket_cx = static_cast<int>(kJacketRect.x + kJacketRect.width * 0.5f) - 45;
        const int jacket_cy = static_cast<int>(kJacketRect.y + kJacketRect.height * 0.5f) - 15;
        DrawText("JACKET", jacket_cx, jacket_cy, 30, Color{126, 130, 138, content_alpha});
    }
    DrawRectangleLinesEx(kJacketRect, 2.0f, Color{196, 200, 208, 255});

    const float detail_x = kJacketRect.x + kJacketRect.width + 20.0f;
    const float detail_max_width = kLeftPanelRect.x + kLeftPanelRect.width - detail_x - 16.0f;
    const double now = GetTime();
    draw_marquee_text(song.song.meta.title.c_str(), static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 4.0f), 40,
                      Color{0, 0, 0, content_alpha}, detail_max_width, now);
    draw_marquee_text(song.song.meta.artist.c_str(), static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 56.0f), 28,
                      Color{80, 80, 80, content_alpha}, detail_max_width, now);
    DrawText(TextFormat("BPM %.0f", song.song.meta.base_bpm), static_cast<int>(detail_x + content_offset_x),
             static_cast<int>(kJacketRect.y + 100.0f), 24, Color{130, 130, 130, content_alpha});
    if (selected_chart != nullptr) {
        DrawText(TextFormat("%s %s Lv.%d", key_mode_label(selected_chart->meta.key_count).c_str(),
                            selected_chart->meta.difficulty.c_str(), selected_chart->meta.level),
                 static_cast<int>(detail_x + content_offset_x), static_cast<int>(kJacketRect.y + 150.0f), 28, Color{0, 0, 0, content_alpha});
        DrawText(selected_chart->meta.chart_author.c_str(), static_cast<int>(detail_x + content_offset_x),
                 static_cast<int>(kJacketRect.y + 186.0f), 20, Color{132, 136, 146, content_alpha});
    }

    DrawText("Songs", static_cast<int>(kSongListRect.x + 20.0f), static_cast<int>(kSongListRect.y + 22.0f), 28, BLACK);
    DrawRectangleRec(kSongListRect, song_list_fill);
    DrawRectangleLinesEx(kSongListRect, 2.0f, panel_border);

    // スクロールオフセットを適用してリストを描画。クリッピング領域外のアイテムはスキップ。
    const float list_top = kSongListRect.y + 60.0f;
    const float list_bottom = kSongListRect.y + kSongListRect.height - 12.0f;
    const Rectangle list_clip = {kSongListRect.x, list_top, kSongListRect.width, list_bottom - list_top};
    BeginScissorMode(static_cast<int>(list_clip.x), static_cast<int>(list_clip.y),
                     static_cast<int>(list_clip.width), static_cast<int>(list_clip.height));
    float item_y = list_top - scroll_y_;
    for (int i = 0; i < static_cast<int>(songs_.size()); ++i) {
        const bool is_selected = i == selected_song_index_;
        float row_h = kRowHeight;
        if (is_selected) {
            row_h = kRowHeight + 14.0f + static_cast<float>(filtered.size()) * 30.0f;
        }

        // 完全に画面外なら描画をスキップ
        if (item_y + row_h < list_top) {
            item_y += row_h;
            continue;
        }
        if (item_y > list_bottom) {
            break;
        }

        const int iy = static_cast<int>(item_y);
        const float row_x = kSongListRect.x + 14.0f;
        const float row_w = kSongListRect.width - 28.0f;
        const int text_x = static_cast<int>(kSongListRect.x + 30.0f);
        const float list_text_max_w = kSongListRect.width - 70.0f;
        const Rectangle row_rect = {row_x, item_y - 8.0f, row_w, 44.0f};
        const bool hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), row_rect);
        if (is_selected) {
            DrawRectangleRec(row_rect, hovered ? Color{214, 220, 227, 255} : Color{223, 228, 234, 255});
        } else if (hovered) {
            DrawRectangleRec(row_rect, Color{236, 240, 245, 255});
        }
        draw_marquee_text(songs_[static_cast<size_t>(i)].song.meta.title.c_str(), text_x, iy, 24,
                          is_selected ? BLACK : DARKGRAY, list_text_max_w, now, &list_clip);
        draw_marquee_text(songs_[static_cast<size_t>(i)].song.meta.artist.c_str(), text_x, iy + 22, 16,
                          Color{132, 136, 146, 255}, list_text_max_w, now, &list_clip);

        if (is_selected) {
            const float child_x = kSongListRect.x + 46.0f;
            const float child_w = kSongListRect.width - 92.0f;
            const int child_text_x = static_cast<int>(kSongListRect.x + 58.0f);
            const int author_x = static_cast<int>(kSongListRect.x + kSongListRect.width - 120.0f);
            float child_y = item_y + 46.0f;
            for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
                const chart_option& chart = *filtered[static_cast<size_t>(chart_index)];
                const bool child_selected = chart_index == difficulty_index_;
                const Rectangle child_rect = {child_x, child_y - 6.0f, child_w, 28.0f};
                const bool child_hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), child_rect);
                if (child_selected) {
                    DrawRectangleRec(child_rect, child_hovered ? Color{214, 220, 227, 255} : Color{223, 228, 234, 255});
                } else if (child_hovered) {
                    DrawRectangleRec(child_rect, Color{236, 240, 245, 255});
                }
                DrawText(TextFormat("%s %s Lv.%d", key_mode_label(chart.meta.key_count).c_str(), chart.meta.difficulty.c_str(),
                                    chart.meta.level),
                         child_text_x, static_cast<int>(child_y), 18,
                         child_selected ? BLACK : DARKGRAY);
                DrawText(chart.meta.chart_author.c_str(), author_x, static_cast<int>(child_y) + 1, 14, Color{132, 136, 146, 255});
                child_y += 30.0f;
            }
        }
        item_y += row_h;
    }
    EndScissorMode();

    // スクロールバー
    const float content_h = compute_content_height();
    const float view_h = list_bottom - list_top;
    const float track_h = kSongListRect.height - 24.0f;
    if (content_h > view_h) {
        const float thumb_h = std::max(36.0f, track_h * (view_h / content_h));
        const float scroll_t = scroll_y_ / std::max(1.0f, content_h - view_h);
        const float thumb_y = kSongListRect.y + 12.0f + (track_h - thumb_h) * scroll_t;
        DrawRectangle(static_cast<int>(kSongListRect.x + kSongListRect.width - 14.0f),
                      static_cast<int>(kSongListRect.y + 12.0f), 6, static_cast<int>(track_h), Color{226, 230, 236, 255});
        DrawRectangle(static_cast<int>(kSongListRect.x + kSongListRect.width - 14.0f),
                      static_cast<int>(thumb_y), 6, static_cast<int>(thumb_h), Color{172, 178, 188, 255});
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
