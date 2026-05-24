#include "multiplayer_result_scene.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "app_paths.h"
#include "audio_manager.h"
#include "path_utils.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr Rectangle kHeaderRect{92.0f, 56.0f, 1260.0f, 96.0f};
constexpr Rectangle kBackButtonRect{1538.0f, 64.0f, 290.0f, 64.0f};
constexpr Rectangle kSummaryPanelRect{92.0f, 184.0f, 500.0f, 760.0f};
constexpr Rectangle kListPanelRect{626.0f, 184.0f, 1202.0f, 760.0f};
constexpr Rectangle kListViewportRect{656.0f, 300.0f, 1142.0f, 600.0f};
constexpr Rectangle kListHeaderRect{656.0f, 244.0f, 1142.0f, 42.0f};
constexpr float kRowHeight = 72.0f;
constexpr float kRowGap = 10.0f;
constexpr float kRefreshSeconds = 1.5f;

std::string result_bgm_path(bool failed) {
    const std::filesystem::path path =
        app_paths::audio_root() / (failed ? "result_failed.mp3" : "result_success.mp3");
    return std::filesystem::exists(path) ? path_utils::to_utf8(path) : "";
}

void play_result_bgm(bool failed) {
    const std::string path = result_bgm_path(failed);
    if (!path.empty() && audio_manager::instance().load_bgm(path)) {
        audio_manager::instance().play_bgm(true);
    }
}

float score_content_height(int row_count) {
    if (row_count <= 0) {
        return kListViewportRect.height;
    }
    return static_cast<float>(row_count) * kRowHeight +
        static_cast<float>(std::max(0, row_count - 1)) * kRowGap;
}

Color rank_color(int rank_index) {
    if (rank_index == 0) {
        return g_theme->all_perfect;
    }
    if (rank_index == 1) {
        return g_theme->slow;
    }
    if (rank_index == 2) {
        return g_theme->fast;
    }
    return g_theme->text_muted;
}

const char* rank_label(rank value) {
    switch (value) {
        case rank::ss: return "SS";
        case rank::s: return "S";
        case rank::aa: return "AA";
        case rank::a: return "A";
        case rank::b: return "B";
        case rank::c: return "C";
        case rank::f: return "F";
    }
    return "F";
}

Color result_rank_color(rank value) {
    switch (value) {
        case rank::ss: return g_theme->rank_ss;
        case rank::s: return g_theme->rank_s;
        case rank::aa: return g_theme->rank_aa;
        case rank::a: return g_theme->rank_a;
        case rank::b: return g_theme->rank_b;
        case rank::c: return g_theme->rank_c;
        case rank::f: return g_theme->rank_f;
    }
    return g_theme->rank_f;
}

void draw_chip(Rectangle rect, const char* text, Color color, int font_size = 18) {
    ui::draw_rect_f(rect, with_alpha(color, 34));
    ui::draw_rect_lines(rect, 1.5f, with_alpha(color, 180));
    ui::draw_text_in_rect(text, font_size, rect, color, ui::text_align::center);
}

void draw_compact_metric(Rectangle rect, const char* label, const char* value, Color value_color) {
    ui::draw_rect_f(rect, g_theme->section);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border_light);
    ui::draw_text_in_rect(label, 16, {rect.x + 12.0f, rect.y + 8.0f, rect.width - 24.0f, 20.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value, 24, {rect.x + 12.0f, rect.y + 28.0f, rect.width - 24.0f, 34.0f},
                          value_color, ui::text_align::left);
}

}  // namespace

multiplayer_result_scene::multiplayer_result_scene(scene_manager& manager,
                                                   result_data result,
                                                   song_data song,
                                                   chart_meta chart,
                                                   int key_count,
                                                   std::string room_id,
                                                   std::string match_id,
                                                   std::string self_user_id,
                                                   std::vector<play_multiplayer_score_row> scores)
    : scene(manager),
      result_(result),
      song_(std::move(song)),
      chart_(std::move(chart)),
      key_count_(key_count),
      room_id_(std::move(room_id)),
      match_id_(std::move(match_id)),
      self_user_id_(std::move(self_user_id)),
      scores_(std::move(scores)) {
    upsert_self_score();
    sort_scores();
}

multiplayer_result_scene::~multiplayer_result_scene() {
    unload_jacket_texture();
}

void multiplayer_result_scene::on_enter() {
    reveal_t_ = 0.0f;
    refresh_t_ = kRefreshSeconds;
    status_message_ = "Match result.";
    load_jacket_texture();
    play_result_bgm(result_.failed);
}

void multiplayer_result_scene::on_exit() {
    unload_jacket_texture();
}

void multiplayer_result_scene::update(float dt) {
    reveal_t_ = std::min(1.0f, reveal_t_ + dt * 1.8f);

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) && !returning_) {
        request_return_to_room();
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !returning_ &&
        CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), kBackButtonRect)) {
        request_return_to_room();
    }

    const float content_height = score_content_height(static_cast<int>(scores_.size()));
    const float max_scroll = std::max(0.0f, content_height - kListViewportRect.height);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (CheckCollisionPointRec(mouse, kListViewportRect)) {
        scroll_y_ = std::clamp(scroll_y_ - GetMouseWheelMove() * 46.0f, 0.0f, max_scroll);
    }
    const Rectangle scrollbar_rect{kListViewportRect.x + kListViewportRect.width + 8.0f,
                                   kListViewportRect.y, 10.0f, kListViewportRect.height};
    const ui::scrollbar_interaction scroll =
        ui::update_vertical_scrollbar(scrollbar_rect, content_height, scroll_y_,
                                      scrollbar_dragging_, scrollbar_drag_offset_);
    if (scroll.changed) {
        scroll_y_ = scroll.scroll_offset;
    }

    poll_room_refresh(dt);

    if (complete_future_.has_value() &&
        complete_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const multiplayer::room_operation_result result = complete_future_->get();
        complete_future_.reset();
        status_message_ = result.success ? "Returning to room..." : result.message;
        manager_.change_scene(song_select::make_multiplayer_title_scene(
            manager_, room_id_, song_.meta.song_id, chart_.chart_id));
    }
}

void multiplayer_result_scene::request_return_to_room() {
    returning_ = true;
    status_message_ = "Returning to room...";
    if (match_id_.empty()) {
        manager_.change_scene(song_select::make_multiplayer_title_scene(
            manager_, room_id_, song_.meta.song_id, chart_.chart_id));
        return;
    }
    if (!complete_future_.has_value()) {
        const auth::session_summary session = auth::load_session_summary();
        const std::string match_id = match_id_;
        complete_future_ = std::async(std::launch::async, [session, match_id]() {
            return multiplayer::client::complete_match(session, match_id);
        });
    }
}

void multiplayer_result_scene::poll_room_refresh(float dt) {
    if (returning_) {
        return;
    }

    if (room_future_.has_value() &&
        room_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const multiplayer::room_operation_result result = room_future_->get();
        room_future_.reset();
        if (result.room.has_value()) {
            apply_live_scores(result.room->live_scores);
        } else if (!result.live_scores.empty()) {
            apply_live_scores(result.live_scores);
        }
    }

    refresh_t_ += dt;
    if (refresh_t_ >= kRefreshSeconds && !room_future_.has_value()) {
        refresh_t_ = 0.0f;
        const auth::session_summary session = auth::load_session_summary();
        const std::string room_id = room_id_;
        room_future_ = std::async(std::launch::async, [session, room_id]() {
            return multiplayer::client::fetch_room(session, room_id);
        });
    }
}

void multiplayer_result_scene::apply_live_scores(const std::vector<multiplayer::live_score>& scores) {
    scores_.clear();
    scores_.reserve(scores.size() + 1);
    for (const multiplayer::live_score& score : scores) {
        scores_.push_back({
            score.user_id,
            score.display_name,
            score.score,
            score.combo,
            score.accuracy,
            score.failed,
        });
    }
    upsert_self_score();
    sort_scores();
}

void multiplayer_result_scene::upsert_self_score() {
    auto self = std::find_if(scores_.begin(), scores_.end(), [this](const play_multiplayer_score_row& score) {
        return !self_user_id_.empty() && score.user_id == self_user_id_;
    });

    const auth::session_summary session = auth::load_session_summary();
    const play_multiplayer_score_row row{
        self_user_id_,
        session.display_name.empty() ? "You" : session.display_name,
        result_.score,
        result_.max_combo,
        result_.accuracy,
        result_.failed,
    };
    if (self == scores_.end()) {
        scores_.push_back(row);
    } else {
        *self = row;
    }
}

void multiplayer_result_scene::sort_scores() {
    std::stable_sort(scores_.begin(), scores_.end(), [](const play_multiplayer_score_row& left,
                                                        const play_multiplayer_score_row& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.accuracy != right.accuracy) {
            return left.accuracy > right.accuracy;
        }
        return left.combo > right.combo;
    });
}

void multiplayer_result_scene::load_jacket_texture() {
    unload_jacket_texture();
    if (song_.directory.empty() || song_.meta.jacket_file.empty()) {
        return;
    }
    const std::filesystem::path jacket_path = std::filesystem::path(song_.directory) / song_.meta.jacket_file;
    if (!std::filesystem::exists(jacket_path)) {
        return;
    }
    jacket_texture_ = LoadTexture(path_utils::to_utf8(jacket_path).c_str());
    jacket_texture_loaded_ = jacket_texture_.id != 0;
}

void multiplayer_result_scene::unload_jacket_texture() {
    if (jacket_texture_loaded_) {
        UnloadTexture(jacket_texture_);
        jacket_texture_ = {};
        jacket_texture_loaded_ = false;
    }
}

void multiplayer_result_scene::draw() {
    draw_scene_background(*g_theme);

    ui::draw_header_block(
        kHeaderRect,
        "MULTIPLAYER RESULT",
        TextFormat("%s / %s  %dK", song_.meta.title.c_str(), chart_.difficulty.c_str(), key_count_),
        42,
        20);

    const Color back_bg = returning_ ? g_theme->row_selected : g_theme->accent;
    if (ui::draw_button_colored(kBackButtonRect, returning_ ? "Returning..." : "Back to Room", 22,
                                back_bg, g_theme->row_hover, g_theme->text).clicked &&
        !returning_) {
        request_return_to_room();
    }

    ui::draw_panel(kSummaryPanelRect);
    const Rectangle jacket_rect{kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 30.0f, 288.0f, 288.0f};
    const Rectangle rank_rect{kSummaryPanelRect.x + 370.0f, kSummaryPanelRect.y + 34.0f, 94.0f, 94.0f};
    if (jacket_texture_loaded_) {
        DrawTexturePro(jacket_texture_,
                       {0.0f, 0.0f, static_cast<float>(jacket_texture_.width), static_cast<float>(jacket_texture_.height)},
                       jacket_rect, {0.0f, 0.0f}, 0.0f, WHITE);
    } else {
        ui::draw_rect_f(jacket_rect, g_theme->section);
    }
    ui::draw_rect_lines(jacket_rect, 2.0f, g_theme->border_image);
    ui::draw_rect_f(rank_rect, with_alpha(result_rank_color(result_.clear_rank), 30));
    ui::draw_rect_lines(rank_rect, 2.0f, result_rank_color(result_.clear_rank));
    ui::draw_text_in_rect(rank_label(result_.clear_rank), 44, rank_rect,
                          result_rank_color(result_.clear_rank), ui::text_align::center);
    draw_chip({rank_rect.x, rank_rect.y + 112.0f, rank_rect.width, 34.0f},
              result_.failed ? "FAILED" : "CLEAR",
              result_.failed ? g_theme->error : g_theme->success, 16);
    if (result_.is_all_perfect) {
        draw_chip({rank_rect.x - 4.0f, rank_rect.y + 160.0f, rank_rect.width + 8.0f, 34.0f},
                  "AP", g_theme->all_perfect, 18);
    } else if (result_.is_full_combo) {
        draw_chip({rank_rect.x - 4.0f, rank_rect.y + 160.0f, rank_rect.width + 8.0f, 34.0f},
                  "FC", g_theme->full_combo, 18);
    }

    ui::draw_text_in_rect("YOUR RESULT", 22,
                          {kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 340.0f, 428.0f, 30.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(std::to_string(result_.score).c_str(), 54,
                          {kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 374.0f, 428.0f, 66.0f},
                          g_theme->text, ui::text_align::left);

    draw_compact_metric({kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 454.0f, 204.0f, 72.0f},
                        "Accuracy", TextFormat("%.2f%%", result_.accuracy), g_theme->fast);
    draw_compact_metric({kSummaryPanelRect.x + 260.0f, kSummaryPanelRect.y + 454.0f, 204.0f, 72.0f},
                        "Max Combo", std::to_string(result_.max_combo).c_str(), g_theme->text);
    draw_compact_metric({kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 536.0f, 204.0f, 72.0f},
                        "Gauge", TextFormat("%.0f%%", result_.gauge_value), g_theme->success);
    draw_compact_metric({kSummaryPanelRect.x + 260.0f, kSummaryPanelRect.y + 536.0f, 204.0f, 72.0f},
                        "RC", TextFormat("%.1f", result_.rc_value), g_theme->accent);

    const Rectangle judge_rect{kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 622.0f, 428.0f, 54.0f};
    const float judge_w = judge_rect.width / 5.0f;
    const char* judge_labels[5] = {"PF", "GR", "GD", "BD", "MS"};
    const Color judge_colors[5] = {
        g_theme->judge_perfect,
        g_theme->judge_great,
        g_theme->judge_good,
        g_theme->judge_bad,
        g_theme->judge_miss,
    };
    for (int i = 0; i < 5; ++i) {
        const Rectangle cell{judge_rect.x + judge_w * static_cast<float>(i), judge_rect.y, judge_w - 6.0f, judge_rect.height};
        ui::draw_text_in_rect(judge_labels[i], 14, {cell.x, cell.y, cell.width, 18.0f},
                              g_theme->text_muted, ui::text_align::center);
        ui::draw_text_in_rect(std::to_string(result_.judge_counts[static_cast<size_t>(i)]).c_str(), 24,
                              {cell.x, cell.y + 20.0f, cell.width, 30.0f},
                              judge_colors[i], ui::text_align::center);
    }
    ui::draw_text_in_rect(TextFormat("Fast %d / Slow %d   Avg %+.1fms",
                                     result_.fast_count, result_.slow_count, result_.avg_offset), 18,
                          {kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 688.0f, 428.0f, 26.0f},
                          g_theme->text_secondary, ui::text_align::left);
    ui::draw_text_in_rect(status_message_.c_str(), 18,
                          {kSummaryPanelRect.x + 36.0f, kSummaryPanelRect.y + 724.0f, 428.0f, 34.0f},
                          g_theme->text_hint, ui::text_align::left);

    ui::draw_panel(kListPanelRect);
    ui::draw_text_in_rect("Ranking", 36,
                          {kListPanelRect.x + 30.0f, kListPanelRect.y + 24.0f, 300.0f, 52.0f},
                          g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d players", static_cast<int>(scores_.size())), 22,
                          {kListPanelRect.x + 330.0f, kListPanelRect.y + 32.0f, 220.0f, 34.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect("SCORE", 18, {kListHeaderRect.x + 536.0f, kListHeaderRect.y, 178.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("ACCURACY", 18, {kListHeaderRect.x + 744.0f, kListHeaderRect.y, 150.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("COMBO", 18, {kListHeaderRect.x + 924.0f, kListHeaderRect.y, 116.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("CLEAR", 18, {kListHeaderRect.x + 1058.0f, kListHeaderRect.y, 84.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);

    const float content_height = score_content_height(static_cast<int>(scores_.size()));
    {
        ui::scoped_clip_rect clip(kListViewportRect);
        float y = kListViewportRect.y - scroll_y_;
        for (int i = 0; i < static_cast<int>(scores_.size()); ++i) {
            const play_multiplayer_score_row& score = scores_[static_cast<size_t>(i)];
            const bool self = !self_user_id_.empty() && score.user_id == self_user_id_;
            const Rectangle row_rect{kListViewportRect.x, y, kListViewportRect.width, kRowHeight};
            if (row_rect.y + row_rect.height >= kListViewportRect.y && row_rect.y <= kListViewportRect.y + kListViewportRect.height) {
                const Color bg = self ? with_alpha(g_theme->row_selected, 245) : g_theme->row;
                ui::draw_row(row_rect, bg, self ? g_theme->row_selected_hover : g_theme->row_hover,
                             self ? g_theme->border_active : g_theme->border, self ? 2.5f : 1.5f);
                ui::draw_text_in_rect(TextFormat("#%d", i + 1), 30,
                                      {row_rect.x + 18.0f, row_rect.y, 74.0f, row_rect.height},
                                      rank_color(i), ui::text_align::left);
                ui::draw_text_in_rect(score.display_name.c_str(), 28,
                                      {row_rect.x + 104.0f, row_rect.y + 4.0f, 360.0f, 40.0f},
                                      score.failed ? g_theme->text_muted : g_theme->text, ui::text_align::left);
                if (self) {
                    draw_chip({row_rect.x + 104.0f, row_rect.y + 42.0f, 60.0f, 24.0f},
                              "YOU", g_theme->accent, 14);
                }
                ui::draw_text_in_rect(std::to_string(score.score).c_str(), 28,
                                      {row_rect.x + 536.0f, row_rect.y, 178.0f, row_rect.height},
                                      g_theme->text, ui::text_align::right);
                ui::draw_text_in_rect(TextFormat("%.2f%%", score.accuracy), 24,
                                      {row_rect.x + 744.0f, row_rect.y, 150.0f, row_rect.height},
                                      g_theme->fast, ui::text_align::right);
                ui::draw_text_in_rect(std::to_string(score.combo).c_str(), 24,
                                      {row_rect.x + 924.0f, row_rect.y, 116.0f, row_rect.height},
                                      g_theme->text_secondary, ui::text_align::right);
                draw_chip({row_rect.x + 1054.0f, row_rect.y + 18.0f, 88.0f, 36.0f},
                          score.failed ? "FAILED" : "CLEAR",
                          score.failed ? g_theme->error : g_theme->success, 16);
            }
            y += kRowHeight + kRowGap;
        }
    }
    ui::draw_scrollbar({kListViewportRect.x + kListViewportRect.width + 8.0f,
                        kListViewportRect.y, 10.0f, kListViewportRect.height},
                       content_height, scroll_y_, g_theme->scrollbar_track, g_theme->scrollbar_thumb);
}
