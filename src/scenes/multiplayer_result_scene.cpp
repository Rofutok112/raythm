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

constexpr Rectangle kBackButtonRect{39.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kLeftPanelRect{39.0f, 109.0f, 330.0f, 854.0f};
constexpr Rectangle kMainPanelRect{390.0f, 109.0f, 820.0f, 932.0f};
constexpr Rectangle kRankingPanelRect{1228.0f, 109.0f, 650.0f, 932.0f};
constexpr Rectangle kJacketRect{69.0f, 127.0f, 270.0f, 270.0f};
constexpr Rectangle kListViewportRect{1258.0f, 246.0f, 590.0f, 754.0f};
constexpr Rectangle kListHeaderRect{1258.0f, 192.0f, 590.0f, 36.0f};
constexpr float kRowHeight = 88.0f;
constexpr float kRowGap = 10.0f;
constexpr float kRefreshSeconds = 1.5f;
constexpr float kSongSelectTopBarHeight = 70.0f;

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

rank parse_rank_label(const std::string& value) {
    if (value == "ss") return rank::ss;
    if (value == "s") return rank::s;
    if (value == "aa") return rank::aa;
    if (value == "a") return rank::a;
    if (value == "b") return rank::b;
    if (value == "c") return rank::c;
    return rank::f;
}

void draw_chip(Rectangle rect, const char* text, Color color, int font_size = 18) {
    ui::draw_rect_f(rect, with_alpha(color, 34));
    ui::draw_rect_lines(rect, 1.5f, with_alpha(color, 180));
    ui::draw_text_in_rect(text, font_size, rect, color, ui::text_align::center);
}

void draw_compact_metric(Rectangle rect, const char* label, const char* value, Color value_color) {
    ui::draw_rect_f(rect, g_theme->section);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border_light);
    ui::draw_text_in_rect(label, 18, {rect.x + 16.0f, rect.y + 12.0f, rect.width - 32.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value, 32, {rect.x + 16.0f, rect.y + 48.0f, rect.width - 32.0f, 42.0f},
                          value_color, ui::text_align::left);
}

void draw_fast_slow_panel(Rectangle rect, bool has_details, int fast_count, int slow_count) {
    ui::draw_rect_f(rect, g_theme->section);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border_light);
    const Rectangle left{rect.x + 26.0f, rect.y + 22.0f, 112.0f, 74.0f};
    const Rectangle right{rect.x + rect.width * 0.5f + 24.0f, rect.y + 22.0f, 112.0f, 74.0f};
    const Color fast_color = has_details ? g_theme->fast : g_theme->text_muted;
    const Color slow_color = has_details ? g_theme->slow : g_theme->text_muted;
    ui::draw_text_in_rect("FAST", 18, {left.x, left.y, left.width, 24.0f}, fast_color, ui::text_align::left);
    ui::draw_rect_f({left.x, left.y + 30.0f, 50.0f, 2.0f}, fast_color);
    ui::draw_text_in_rect(has_details ? std::to_string(fast_count).c_str() : "--", 34,
                          {left.x, left.y + 42.0f, left.width, 38.0f}, fast_color, ui::text_align::left);
    ui::draw_line_ex({rect.x + rect.width * 0.5f, rect.y + 24.0f},
                     {rect.x + rect.width * 0.5f, rect.y + rect.height - 24.0f},
                     1.0f, g_theme->border);
    ui::draw_text_in_rect("SLOW", 18, {right.x, right.y, right.width, 24.0f}, slow_color, ui::text_align::left);
    ui::draw_rect_f({right.x, right.y + 30.0f, 50.0f, 2.0f}, slow_color);
    ui::draw_text_in_rect(has_details ? std::to_string(slow_count).c_str() : "--", 34,
                          {right.x, right.y + 42.0f, right.width, 38.0f}, slow_color, ui::text_align::left);
}

void draw_background() {
    draw_scene_background(*g_theme);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight,
                           with_alpha(g_theme->panel, 120),
                           with_alpha(g_theme->bg_alt, 245));
    for (int x = 0; x < kScreenWidth; x += 32) {
        ui::draw_line_ex({static_cast<float>(x), 0.0f}, {static_cast<float>(x), static_cast<float>(kScreenHeight)},
                         1.0f, with_alpha(g_theme->border_light, 22));
    }
    for (int y = 0; y < kScreenHeight; y += 32) {
        ui::draw_line_ex({0.0f, static_cast<float>(y)}, {static_cast<float>(kScreenWidth), static_cast<float>(y)},
                         1.0f, with_alpha(g_theme->border_light, 16));
    }
}

void draw_song_select_top_bar() {
    const Rectangle visible = virtual_screen::visible_rect();
    const Rectangle top_bar{visible.x, visible.y, visible.width, kSongSelectTopBarHeight};
    const Color bar_color = lerp_color(g_theme->panel, BLACK, 0.58f);
    ui::draw_rect_f(top_bar, with_alpha(bar_color, 235));
    ui::draw_rect_f({top_bar.x, top_bar.y + top_bar.height - 2.0f, top_bar.width, 2.0f},
                    with_alpha(g_theme->border, 150));
}

void draw_result_panel(Rectangle rect, Color border = {0, 0, 0, 0}) {
    const Color resolved_border = border.a > 0 ? border : g_theme->border;
    ui::draw_rect_f(rect, with_alpha(g_theme->panel, 214));
    ui::draw_rect_lines(rect, 1.5f, resolved_border);
}

void draw_song_select_column(Rectangle rect) {
    const unsigned char fill_alpha =
        static_cast<unsigned char>(g_theme->row_soft_alpha / 2);
    ui::draw_rect_f(rect, with_alpha(g_theme->section, fill_alpha));
    ui::draw_rect_lines(rect, 1.2f, with_alpha(g_theme->border_light, 255));
}

void draw_chart_badges(Rectangle rect, const chart_meta& chart, int key_count) {
    const Color level_color = difficulty_level_color(chart.level);
    const Rectangle key_chip{rect.x, rect.y, 42.0f, 32.0f};
    const Rectangle difficulty_rect{key_chip.x + key_chip.width + 14.0f, rect.y - 1.0f, 116.0f, 34.0f};
    const Rectangle level_rect{rect.x + rect.width - 78.0f, rect.y, 78.0f, 32.0f};
    ui::draw_rect_f(key_chip, with_alpha(lerp_color(g_theme->section, level_color, 0.18f), 224));
    ui::draw_rect_lines(key_chip, 1.0f, with_alpha(level_color, 184));
    ui::draw_text_in_rect(TextFormat("%dK", key_count), 15, key_chip, g_theme->text, ui::text_align::center);
    draw_marquee_text(chart.difficulty.c_str(), difficulty_rect, 22, g_theme->text, GetTime());
    draw_difficulty_level_badge(chart.level, level_rect, 15, 255);
}

std::string format_score(int score) {
    std::string value = std::to_string(std::max(0, score));
    for (int i = static_cast<int>(value.size()) - 3; i > 0; i -= 3) {
        value.insert(static_cast<size_t>(i), ",");
    }
    return value;
}

int find_self_placement(const std::vector<play_multiplayer_score_row>& scores, const std::string& self_user_id) {
    for (int i = 0; i < static_cast<int>(scores.size()); ++i) {
        if (!self_user_id.empty() && scores[static_cast<size_t>(i)].user_id == self_user_id) {
            return i + 1;
        }
    }
    return 0;
}

std::string score_key(const play_multiplayer_score_row& score) {
    return !score.user_id.empty() ? score.user_id : score.display_name;
}

int find_score_placement(const std::vector<play_multiplayer_score_row>& scores, const std::string& key) {
    for (int i = 0; i < static_cast<int>(scores.size()); ++i) {
        if (score_key(scores[static_cast<size_t>(i)]) == key) {
            return i + 1;
        }
    }
    return 0;
}

const play_multiplayer_score_row* find_score(const std::vector<play_multiplayer_score_row>& scores,
                                             const std::string& key) {
    const auto it = std::find_if(scores.begin(), scores.end(), [&key](const play_multiplayer_score_row& score) {
        return score_key(score) == key;
    });
    return it == scores.end() ? nullptr : &*it;
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
    if (!self_user_id_.empty()) {
        selected_score_key_ = self_user_id_;
    }
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

void multiplayer_result_scene::on_app_exit() {
    if (room_id_.empty()) {
        return;
    }
    const auth::session_summary session = auth::load_session_summary();
    if (!match_id_.empty()) {
        (void)multiplayer::client::complete_match(session, match_id_);
    }
    (void)multiplayer::client::leave_room(session, room_id_);
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
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const float local_y = mouse.y - kListViewportRect.y + scroll_y_;
            const int index = static_cast<int>(local_y / (kRowHeight + kRowGap));
            const float row_local_y = local_y - static_cast<float>(index) * (kRowHeight + kRowGap);
            if (index >= 0 && index < static_cast<int>(scores_.size()) && row_local_y <= kRowHeight) {
                selected_score_key_ = score_key(scores_[static_cast<size_t>(index)]);
            }
        }
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
            score.has_result_details,
            score.judge_counts,
            score.rc_value,
            score.avg_offset,
            score.fast_count,
            score.slow_count,
            score.clear_rank.empty() ? rank::f : parse_rank_label(score.clear_rank),
            score.is_full_combo,
            score.is_all_perfect,
        });
    }
    upsert_self_score();
    sort_scores();
    if (find_score(scores_, selected_score_key_) == nullptr) {
        selected_score_key_ = !self_user_id_.empty() ? self_user_id_ :
            (scores_.empty() ? std::string{} : score_key(scores_.front()));
    }
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
        true,
        result_.judge_counts,
        result_.rc_value,
        result_.avg_offset,
        result_.fast_count,
        result_.slow_count,
        result_.clear_rank,
        result_.is_full_combo,
        result_.is_all_perfect,
    };
    if (self == scores_.end()) {
        scores_.push_back(row);
    } else {
        *self = row;
    }
    if (selected_score_key_.empty()) {
        selected_score_key_ = score_key(row);
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
    virtual_screen::begin_ui();

    draw_background();
    draw_song_select_top_bar();

    draw_song_select_column(kLeftPanelRect);
    draw_song_select_column(kMainPanelRect);
    draw_song_select_column(kRankingPanelRect);

    const play_multiplayer_score_row fallback_score{
        self_user_id_,
        "You",
        result_.score,
        result_.max_combo,
        result_.accuracy,
        result_.failed,
        true,
        result_.judge_counts,
        result_.rc_value,
        result_.avg_offset,
        result_.fast_count,
        result_.slow_count,
        result_.clear_rank,
        result_.is_full_combo,
        result_.is_all_perfect,
    };
    const play_multiplayer_score_row* selected = find_score(scores_, selected_score_key_);
    if (selected == nullptr) {
        selected = &fallback_score;
    }
    const std::string self_key = !self_user_id_.empty() ? self_user_id_ : score_key(fallback_score);
    const bool selected_self = score_key(*selected) == self_key;
    const bool selected_has_details = selected_self || selected->has_result_details;
    const rank selected_rank = selected_has_details
        ? selected->clear_rank
        : (selected->failed ? rank::f : compute_rank(selected->accuracy, false));
    const Color selected_rank_color = result_rank_color(selected_rank);
    const int selected_place = find_score_placement(scores_, score_key(*selected));

    const Rectangle jacket_rect = kJacketRect;
    if (jacket_texture_loaded_) {
        DrawTexturePro(jacket_texture_,
                       {0.0f, 0.0f, static_cast<float>(jacket_texture_.width), static_cast<float>(jacket_texture_.height)},
                       jacket_rect, {0.0f, 0.0f}, 0.0f, WHITE);
    } else {
        ui::draw_rect_f(jacket_rect, g_theme->section);
    }
    ui::draw_rect_lines(jacket_rect, 2.0f, g_theme->border_image);

    draw_marquee_text(song_.meta.title.c_str(), {69.0f, 424.0f, 270.0f, 48.0f}, 32,
                      g_theme->text, GetTime());
    draw_marquee_text(song_.meta.artist.c_str(), {69.0f, 476.0f, 270.0f, 34.0f}, 22,
                      g_theme->text_secondary, GetTime());

    draw_chart_badges({69.0f, 534.0f, 270.0f, 32.0f}, chart_, key_count_);

    const int self_place = find_self_placement(scores_, self_user_id_);
    ui::draw_rect_f({69.0f, 610.0f, 270.0f, 112.0f}, with_alpha(g_theme->section, 230));
    ui::draw_rect_lines({69.0f, 610.0f, 270.0f, 112.0f}, 1.5f, g_theme->border_light);
    ui::draw_text_in_rect("YOUR PLACE", 18, {89.0f, 628.0f, 230.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(self_place > 0 ? TextFormat("#%d", self_place) : "--", 52,
                          {89.0f, 656.0f, 116.0f, 54.0f},
                          self_place > 0 ? rank_color(self_place - 1) : g_theme->text_muted,
                          ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d players", static_cast<int>(scores_.size())), 24,
                          {206.0f, 670.0f, 114.0f, 34.0f}, g_theme->text_secondary,
                          ui::text_align::right);
    ui::draw_text_in_rect(status_message_.c_str(), 18, {69.0f, 774.0f, 270.0f, 74.0f},
                          g_theme->text_hint, ui::text_align::left);

    ui::draw_text_in_rect(selected->display_name.c_str(), 26,
                          {430.0f, 127.0f, 720.0f, 34.0f}, g_theme->text_secondary,
                          ui::text_align::left);
    const Rectangle rank_rect{430.0f, 142.0f, 254.0f, 190.0f};
    ui::draw_text_in_rect(rank_label(selected_rank), selected_rank == rank::aa ? 108 : 138,
                          rank_rect, selected_rank_color, ui::text_align::center);
    ui::draw_line_ex({704.0f, 160.0f}, {704.0f, 322.0f}, 1.5f, g_theme->border);

    ui::draw_text_in_rect(format_score(selected->score).c_str(), 82,
                          {736.0f, 152.0f, 414.0f, 96.0f},
                          g_theme->text, ui::text_align::right);
    ui::draw_rect_f({736.0f, 270.0f, 414.0f, 3.0f}, selected_rank_color);
    ui::draw_text_in_rect(selected_has_details ? TextFormat("RC %.1f", selected->rc_value) : "RC --", 28,
                          {736.0f, 284.0f, 190.0f, 42.0f}, g_theme->text_secondary,
                          ui::text_align::left);
    const char* clear_label = selected->failed ? "FAILED" :
        (selected_has_details && selected->is_all_perfect ? "ALL PERFECT" :
         (selected_has_details && selected->is_full_combo ? "FULL COMBO" : "CLEAR"));
    const Color clear_color = selected->failed ? g_theme->error :
        (selected_has_details && selected->is_all_perfect ? g_theme->all_perfect :
         (selected_has_details && selected->is_full_combo ? g_theme->full_combo : g_theme->success));
    ui::draw_text_in_rect(clear_label, 28, {942.0f, 284.0f, 208.0f, 42.0f},
                          clear_color, ui::text_align::right);

    draw_compact_metric({430.0f, 376.0f, 344.0f, 118.0f},
                        "Accuracy", TextFormat("%.2f%%", selected->accuracy), g_theme->fast);
    draw_compact_metric({806.0f, 376.0f, 344.0f, 118.0f},
                        "Max Combo", std::to_string(selected->combo).c_str(), g_theme->accent);
    draw_compact_metric({430.0f, 518.0f, 344.0f, 118.0f},
                        "Avg Offset", selected_has_details ? TextFormat("%+.1fms", selected->avg_offset) : "--",
                        g_theme->text_secondary);
    draw_fast_slow_panel({806.0f, 518.0f, 344.0f, 118.0f},
                         selected_has_details, selected->fast_count, selected->slow_count);

    const Rectangle judge_rect{430.0f, 674.0f, 720.0f, 330.0f};
    draw_result_panel(judge_rect);
    const Rectangle judge_content = ui::inset(judge_rect, ui::edge_insets::symmetric(24.0f, 22.0f));
    const char* judge_labels[5] = {"Perfect", "Great", "Good", "Bad", "Miss"};
    const Color judge_colors[5] = {
        g_theme->judge_perfect,
        g_theme->judge_great,
        g_theme->judge_good,
        g_theme->judge_bad,
        g_theme->judge_miss,
    };
    const float judge_row_h = judge_content.height / 5.0f;
    for (int i = 0; i < 5; ++i) {
        const Rectangle row{judge_content.x, judge_content.y + judge_row_h * static_cast<float>(i),
                            judge_content.width, judge_row_h};
        ui::draw_text_in_rect(judge_labels[i], 26, {row.x, row.y, 220.0f, row.height},
                              selected_has_details ? judge_colors[i] : g_theme->text_muted, ui::text_align::left);
        const std::string count_text = selected_has_details
            ? std::to_string(selected->judge_counts[static_cast<size_t>(i)])
            : "--";
        ui::draw_text_in_rect(count_text.c_str(), 34,
                              {row.x + 240.0f, row.y, row.width - 240.0f, row.height},
                              g_theme->text, ui::text_align::right);
        if (i < 4) {
            ui::draw_line_ex({row.x, row.y + row.height}, {row.x + row.width, row.y + row.height},
                             1.0f, g_theme->border_light);
        }
    }

    ui::draw_text_in_rect("Ranking", 38,
                          {kRankingPanelRect.x + 30.0f, kRankingPanelRect.y + 18.0f, 260.0f, 54.0f},
                          g_theme->text, ui::text_align::left);
    ui::draw_rect_f({kRankingPanelRect.x + 30.0f, kRankingPanelRect.y + 80.0f, 590.0f, 3.0f}, g_theme->border);
    ui::draw_text_in_rect(TextFormat("%d players", static_cast<int>(scores_.size())), 22,
                          {kRankingPanelRect.x + 402.0f, kRankingPanelRect.y + 30.0f, 176.0f, 34.0f},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("SCORE", 16, {kListHeaderRect.x + 248.0f, kListHeaderRect.y, 112.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("ACC", 16, {kListHeaderRect.x + 374.0f, kListHeaderRect.y, 78.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);
    ui::draw_text_in_rect("COMBO", 16, {kListHeaderRect.x + 464.0f, kListHeaderRect.y, 78.0f, kListHeaderRect.height},
                          g_theme->text_muted, ui::text_align::right);

    const float content_height = score_content_height(static_cast<int>(scores_.size()));
    {
        ui::scoped_clip_rect clip(kListViewportRect);
        float y = kListViewportRect.y - scroll_y_;
        for (int i = 0; i < static_cast<int>(scores_.size()); ++i) {
            const play_multiplayer_score_row& score = scores_[static_cast<size_t>(i)];
            const bool self = !self_user_id_.empty() && score.user_id == self_user_id_;
            const bool selected_row = score_key(score) == selected_score_key_;
            const Rectangle row_rect{kListViewportRect.x, y, kListViewportRect.width, kRowHeight};
            if (row_rect.y + row_rect.height >= kListViewportRect.y && row_rect.y <= kListViewportRect.y + kListViewportRect.height) {
                const Color bg = selected_row
                    ? with_alpha(g_theme->row_soft_selected, 245)
                    : (self ? with_alpha(g_theme->row_selected, 218) : g_theme->row);
                ui::draw_row(row_rect, bg, selected_row ? g_theme->row_soft_selected_hover : g_theme->row_hover,
                             selected_row ? g_theme->border_active : g_theme->border,
                             selected_row ? 3.0f : 1.5f);
                ui::draw_text_in_rect(TextFormat("#%d", i + 1), 27,
                                      {row_rect.x + 18.0f, row_rect.y, 70.0f, row_rect.height},
                                      rank_color(i), ui::text_align::left);
                ui::draw_text_in_rect(score.display_name.c_str(), 26,
                                      {row_rect.x + 92.0f, row_rect.y + 7.0f, 156.0f, 38.0f},
                                      score.failed ? g_theme->text_muted : g_theme->text, ui::text_align::left);
                if (self) {
                    draw_chip({row_rect.x + 92.0f, row_rect.y + 50.0f, 56.0f, 22.0f},
                              "YOU", g_theme->accent, 13);
                }
                ui::draw_text_in_rect(format_score(score.score).c_str(), 22,
                                      {row_rect.x + 248.0f, row_rect.y, 112.0f, row_rect.height},
                                      g_theme->text, ui::text_align::right);
                ui::draw_text_in_rect(TextFormat("%.1f%%", score.accuracy), 20,
                                      {row_rect.x + 374.0f, row_rect.y, 78.0f, row_rect.height},
                                      g_theme->fast, ui::text_align::right);
                ui::draw_text_in_rect(std::to_string(score.combo).c_str(), 20,
                                      {row_rect.x + 464.0f, row_rect.y, 78.0f, row_rect.height},
                                      g_theme->text_secondary, ui::text_align::right);
                ui::draw_rect_f({row_rect.x, row_rect.y, 4.0f, row_rect.height},
                                selected_row ? rank_color(i) : (score.failed ? g_theme->error : g_theme->success));
            }
            y += kRowHeight + kRowGap;
        }
    }
    ui::draw_scrollbar({kListViewportRect.x + kListViewportRect.width + 8.0f,
                        kListViewportRect.y, 10.0f, kListViewportRect.height},
                       content_height, scroll_y_, g_theme->scrollbar_track, g_theme->scrollbar_thumb);

    const unsigned char normal_row_alpha = g_theme->row_soft_alpha;
    const unsigned char hover_row_alpha = g_theme->row_soft_hover_alpha;
    const Color back_bg = returning_ ? g_theme->row_soft_selected : g_theme->row_soft;
    if (ui::draw_button_colored(kBackButtonRect, returning_ ? "Returning..." : "Back to Room", 24,
                                with_alpha(back_bg, normal_row_alpha),
                                with_alpha(g_theme->row_soft_hover, hover_row_alpha),
                                g_theme->text, 1.5f).clicked &&
        !returning_) {
        request_return_to_room();
    }

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}


